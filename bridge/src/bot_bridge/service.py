from __future__ import annotations
import argparse,asyncio,fcntl,json,os,secrets,signal,subprocess
from pathlib import Path
from aiohttp import web
from .runtime import Ledger,AGENTS,now
from .adapters import Adapters
from .usb import USB
from .usage import UsageStore,UsageCollector
VERSION='3.0.0'
DEFAULT_ROOT=Path.home()/'.local/share/agent-robot-face'

class Service:
    def __init__(self,root=DEFAULT_ROOT,port=17940,serial_port=None,read_sources=True):
        self.root=Path(root);self.l=Ledger(root);self.port=port;self.usb=USB(self.l,serial_port);self.adapters=Adapters(self.l);self.read_sources=read_sources;self.errors=[]
        self.usage=UsageStore(root);self.usb.usage=self.usage;self.usage_collector=UsageCollector(self.usage,Path(__file__).resolve().parents[3])
        self.lock=(self.root/'service.lock').open('a');fcntl.flock(self.lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        p=self.root/'control.token'
        if not p.exists():p.write_text(secrets.token_urlsafe(32));p.chmod(0o600)
        self.token=p.read_text().strip();self.tasks=[]
    def state(self):
        sources=[];manifest=self.l.get('installation',{})
        for c in self.l.catalog()['agents']:
            a=c['id'];s=self.l.source(a);observed=bool(s['observed']);age=now()-s['last_event'] if s['last_event'] else None
            caps={k:'unverified' for k in ('start','tool','success','failure','cancellation','waiting','usage','quota')}
            evidence={r[0] for r in self.l.db.execute('SELECT DISTINCT kind FROM events WHERE agent=?',(a,))}
            for key,kind in [('start','start'),('tool','tool_start'),('success','done'),('failure','error'),('cancellation','cancelled'),('waiting','waiting')]:
                caps[key]='observed' if kind in evidence else 'unverified'
            caps['usage']='observed' if self.l.db.execute('SELECT 1 FROM usage WHERE agent=? LIMIT 1',(a,)).fetchone() else 'unverified'
            usage=self.usage.query(a,'30d')
            caps['usage']='observed' if usage['summary']['total_tokens'] is not None else caps['usage']
            caps['quota']='observed' if any(q['availability']=='available' for q in usage['quotas']) else 'unavailable'
            sources.append(dict(id=a,label=c['label'],installed=(Path.home()/('.'+('workbuddy' if a=='workbuddy' else a))).exists(),configured=a in manifest.get('sources',[]),observed=observed,running=observed and self.l.active_count(a)>0,health=c['health'],last_event_ms=s['last_event'],last_scan_ms=s['last_scan'],stale=age is not None and age>120000,active_sessions=self.l.active_count(a) if observed and not self.l.synchronizing.get(a) else None,synchronizing=bool(self.l.synchronizing.get(a)),backlog_bytes=self.l.synchronizing.get(a,0),capabilities=caps,validated_coverage={'codex':['desktop_rollout','cli_start_tool_success_failure_interrupt'],'cursor':['native_completed_aborted'],'hermes':['cli_db_success'],'workbuddy':['desktop_jsonl_start_tool_success']}[a],gaps=([] if caps['quota']=='observed' else ['official_quota_unavailable'])+['waiting_unverified','bounded_2day_backfill_partial','silent_active_liveness_unverified']+(['source_syncing_history'] if self.l.synchronizing.get(a) else [])+(['oversized_record_skipped'] if self.l.get('oversized_record_'+a) else [])+([s['error']] if s['error'] else [])+(['native_terminal_unverified','cancellation_ambiguous'] if a=='workbuddy' else [])))
        events=[dict(r) for r in self.l.db.execute('SELECT agent,session,run,kind,ts,tool,channel FROM events ORDER BY ts DESC LIMIT 20')]
        return dict(api_version=1,bridge=dict(version=VERSION,started_at_ms=self.l.started,paused=self.l.paused,launch_at_login=self.l.get('launch_at_login',False),demo=False),device=self.usb.state,selection=self.l.selection,sources=sources,focus=self.l.focus(),stats=self.l.stats(),diagnostics=dict(event_count=self.l.db.execute('SELECT count(*) FROM events').fetchone()[0],recent_events=events,errors=self.errors[-20:]))
    def guard(self,r,mutation=False):
        if r.host not in (f'127.0.0.1:{self.port}',f'localhost:{self.port}') or r.headers.get('Origin'):return web.json_response({'ok':False,'error':'forbidden'},status=403)
        if mutation and not secrets.compare_digest(r.headers.get('Authorization',''),'Bearer '+self.token):return web.json_response({'ok':False,'error':'unauthorized'},status=401)
    async def get_state(self,r):
        error=self.guard(r)
        if error is not None:return error
        return web.json_response(self.state())
    async def get_usage(self,r):
        error=self.guard(r)
        if error is not None:return error
        try:
            if set(r.query)-{'subject','period'}:raise ValueError()
            subject=r.query.get('subject','current')
            if subject=='current':subject=self.l.selection['selected_agent']
            return web.json_response(self.usage.query(subject,r.query.get('period','today')))
        except ValueError:return web.json_response({'ok':False,'error':'invalid'},status=400)
    async def refresh_usage(self,r):
        error=self.guard(r,True)
        if error is not None:return error
        try:
            if r.content_type!='application/json' or await r.json()!={}:raise ValueError()
        except (ValueError,TypeError):return web.json_response({'ok':False,'error':'invalid'},status=400)
        self.usage_collector.refresh()
        return web.json_response({'ok':True,'status':'queued'},status=202)
    async def control(self,r):
        error=self.guard(r,True)
        if error is not None:return error
        try:
            if r.content_type!='application/json':raise ValueError()
            d=await r.json();action=d.get('action')
            allowed={'auto':{'action','expected_selection_rev'},'pin':{'action','agent_id','expected_selection_rev'},'pause':{'action'},'resume':{'action'},'launch_at_login':{'action','enabled'}}
            if action not in allowed or set(d)!=allowed[action]:raise ValueError()
            if action in ('auto','pin'):
                if type(d['expected_selection_rev']) is not int:raise ValueError()
                if d['expected_selection_rev']!=self.l.selection['selection_rev']:return web.json_response({'ok':False,'error':'conflict'},status=409)
                agent=d.get('agent_id',self.l.selection['selected_agent'])
                if agent not in AGENTS:raise ValueError()
                self.l.select('auto' if action=='auto' else 'pinned',agent);self.l.auto()
            elif action in ('pause','resume'):
                self.l.paused=action=='pause';self.l.put('paused',self.l.paused)
                if self.l.paused:
                    # Serial owner acknowledges release by its next iteration.
                    for _ in range(30):
                        if self.usb.serial is None:break
                        await asyncio.sleep(.05)
                    if self.usb.serial is not None:return web.json_response({'ok':False,'error':'internal_error'},status=500)
            else:
                if type(d['enabled']) is not bool:raise ValueError()
                result=await asyncio.create_subprocess_exec('/bin/launchctl','enable' if d['enabled'] else 'disable',f'gui/{os.getuid()}/com.agentrobotface.bridge',stdout=asyncio.subprocess.DEVNULL,stderr=asyncio.subprocess.DEVNULL)
                if await result.wait()!=0:return web.json_response({'ok':False,'error':'internal_error'},status=500)
                self.l.put('launch_at_login',d['enabled'])
            return web.json_response(dict(ok=True,selection=self.l.selection,paused=self.l.paused))
        except (ValueError,TypeError,AttributeError):return web.json_response({'ok':False,'error':'invalid'},status=400)
    async def poll(self):
        while True:
            try:
                if self.read_sources:self.adapters.poll()
                self.l.auto()
            except Exception as e:
                self.errors.append(dict(kind=type(e).__name__,at_ms=now()));self.errors=self.errors[-20:]
            await asyncio.sleep(1)
    async def run(self):
        app=web.Application(client_max_size=4096);app.router.add_get('/v1/state',self.get_state);app.router.add_post('/v1/control',self.control)
        app.router.add_get('/v1/usage',self.get_usage);app.router.add_post('/v1/usage/refresh',self.refresh_usage)
        runner=web.AppRunner(app,access_log=None);await runner.setup();await web.TCPSite(runner,'127.0.0.1',self.port).start()
        self.tasks=[asyncio.create_task(self.poll()),asyncio.create_task(self.usb.run())];done=asyncio.Event()
        if self.read_sources:self.tasks.append(asyncio.create_task(self.usage_collector.run()))
        for sig in (signal.SIGTERM,signal.SIGINT):asyncio.get_running_loop().add_signal_handler(sig,done.set)
        await done.wait();self.usb.stopping=True
        await self.usage_collector.close()
        for t in self.tasks:t.cancel()
        await asyncio.gather(*self.tasks,return_exceptions=True)
        self.usb.close();await runner.cleanup();self.l.db.close();self.usage.db.close()

def main():
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=DEFAULT_ROOT);p.add_argument('--port',type=int,default=17940);p.add_argument('--serial-port');p.add_argument('--no-sources',action='store_true',help='Explicit integration mode; requires a separate store');a=p.parse_args()
    if a.no_sources and a.root==DEFAULT_ROOT:p.error('--no-sources requires an isolated --root')
    asyncio.run(Service(a.root,a.port,a.serial_port,not a.no_sources).run())
if __name__=='__main__':main()
