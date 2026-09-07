"""Independent, metadata-only usage archive. Business lifecycle never reads this store."""
from __future__ import annotations
import asyncio,datetime,json,math,os,re,sqlite3,time,signal,shutil
from pathlib import Path
from .runtime import AGENTS,now
FIELDS=('input_tokens','output_tokens','cache_read_tokens','cache_write_tokens','total_tokens')
PERIODS={'today':1,'7d':7,'30d':30}
MAX_INTEGER=9007199254740991

def number(v):
    return type(v) in (int,float) and math.isfinite(v) and 0<=v<=MAX_INTEGER

def token(v):return v is None or (type(v) is int and 0<=v<=MAX_INTEGER)
def label(v,limit=96):return isinstance(v,str) and len(v)<=limit and all(ord(c)>=32 for c in v)
def summed(rows,key):
    values=[r[key] for r in rows if r.get(key) is not None]
    return sum(values) if values and sum(values)<=MAX_INTEGER else None

class UsageStore:
    def __init__(self,root):
        self.root=Path(root);self.root.mkdir(parents=True,exist_ok=True)
        self.db=sqlite3.connect(self.root/'usage-v3.sqlite3');self.db.row_factory=sqlite3.Row
        self.db.executescript('''PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS days(date TEXT,agent TEXT,model TEXT,data TEXT,asof INTEGER,PRIMARY KEY(date,agent,model));
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT);
''')
        self.status='starting';self.error=None;self.collecting=False
    def meta(self,key,default=None):
        r=self.db.execute('SELECT value FROM meta WHERE key=?',(key,)).fetchone();return json.loads(r[0]) if r else default
    def put(self,key,value):self.db.execute('INSERT OR REPLACE INTO meta VALUES(?,?)',(key,json.dumps(value,separators=(',',':'))))
    def ingest(self,snapshot):
        if not isinstance(snapshot,dict) or snapshot.get('version')!=1:raise ValueError('collector_version')
        rows=snapshot.get('days');asof=snapshot.get('as_of_ms');zone=snapshot.get('timezone')
        if not isinstance(rows,list) or len(rows)>10000 or type(asof) is not int or not 0<asof<=MAX_INTEGER or not label(zone,64):raise ValueError('collector_shape')
        clean=[];seen=set()
        for r in rows:
            if not isinstance(r,dict) or r.get('agent') not in AGENTS:raise ValueError('collector_agent')
            date=datetime.date.fromisoformat(r.get('date',''));model=r.get('model') or 'unknown'
            if not label(model) or not all(token(r.get(k)) for k in FIELDS):raise ValueError('collector_metric')
            key=(date.isoformat(),r['agent'],model)
            if key in seen:raise ValueError('collector_duplicate_bucket')
            seen.add(key);coverage=r.get('coverage') or {}
            row=dict(date=key[0],agent=key[1],model=model,**{k:r.get(k) for k in FIELDS},coverage={k:coverage.get(k) if coverage.get(k) in ('complete','partial','unknown') else 'unknown' for k in ('tokens','cost')},actual_cost=None)
            c=r.get('actual_cost')
            if c is not None:
                if not isinstance(c,dict) or not number(c.get('amount')) or not re.fullmatch('[A-Z]{3}',str(c.get('currency',''))) or not label(c.get('provenance'),64):raise ValueError('collector_cost')
                # Collector contract requires an explicit native actual-cost field. Never accept
                # generic tokscale pricing, estimates or mixed values by name coincidence.
                if c['provenance'] not in ('hermes_actual_cost_usd','cursor_official_actual_cost','workbuddy_native_actual_cost','codex_native_actual_cost'):raise ValueError('cost_provenance_unverified')
                row['actual_cost']={k:c[k] for k in ('amount','currency','provenance')}
            row_asof=r.get('as_of_ms',asof)
            if type(row_asof) is not int or not 0<row_asof<=asof:raise ValueError('collector_freshness')
            row['_as_of_ms']=row_asof
            clean.append(row)
        quotas=self.validate_quotas(snapshot.get('quotas',[]))
        sources=snapshot.get('sources',[])
        # Keep only bounded status metadata; never retain arbitrary stdout/error bodies.
        source_rows=[]
        if isinstance(sources,dict):sources=[dict(v,id=k) for k,v in sources.items() if isinstance(v,dict)]
        for s in sources if isinstance(sources,list) else []:
            if isinstance(s,dict) and s.get('id',s.get('agent')) in AGENTS:
                source_rows.append({'id':s.get('id',s.get('agent')),'availability':s.get('availability') if s.get('availability') in ('available','unavailable','error','needs_auth','partial') else 'partial','coverage':s.get('coverage') if s.get('coverage') in ('complete','partial','unknown') else 'partial',
                    'as_of_ms':s.get('as_of_ms',asof) if token(s.get('as_of_ms',asof)) else None,
                    'provenance':s.get('provenance') if s.get('provenance') in ('tokscale.local_logs','cursor.official.usage-events') else 'unknown',
                    'reason':s.get('reason') if isinstance(s.get('reason'),str) and re.fullmatch('[a-z_]{1,64}',s['reason']) else None})
        failure=bool(snapshot.get('errors'))
        with self.db:
            if asof<self.meta('as_of_ms',0):return
            replacement=snapshot.get('range') or {}
            full_agents={src.get('id',src.get('agent')) for src in sources if isinstance(src,dict) and src.get('snapshot_complete') is True and src.get('availability')=='available'} if isinstance(sources,list) else set()
            if full_agents and not failure:
                lo=datetime.date.fromisoformat(replacement.get('since',''));hi=datetime.date.fromisoformat(replacement.get('until',''))
                if not 0<=(hi-lo).days<30:raise ValueError('snapshot_range')
                for agent in full_agents & set(AGENTS):self.db.execute('DELETE FROM days WHERE agent=? AND date>=? AND date<=?',(agent,lo.isoformat(),hi.isoformat()))
            for date,agent in {(r['date'],r['agent']) for r in clean}:self.db.execute('DELETE FROM days WHERE date=? AND agent=?',(date,agent))
            for r in clean:self.db.execute('INSERT OR REPLACE INTO days VALUES(?,?,?,?,?)',(r['date'],r['agent'],r['model'],json.dumps(r,separators=(',',':')),r['_as_of_ms']))
            self.put('attempt_ms',asof)
            if not failure:self.put('as_of_ms',asof)
            self.put('timezone',zone);self.put('quotas',quotas);self.put('sources',source_rows);self.put('revision',self.meta('revision',0)+1)
        self.status='error' if failure else 'ready';self.error='collector_read_failed' if failure else None
    def validate_quotas(self,quotas):
        if not isinstance(quotas,list) or len(quotas)>32:raise ValueError('quota_limit')
        out=[];seen=set()
        for q in quotas:
            if not isinstance(q,dict):raise ValueError('quota_shape')
            account=q.get('account_hash');window=q.get('window_key');provider=q.get('provider');agents=q.get('agents',[])
            if not label(provider,32) or not label(window,64):raise ValueError('quota_identity')
            if account is None and q.get('availability')!='available':pass
            elif not isinstance(account,str) or not re.fullmatch('[0-9a-f]{16,64}',account):raise ValueError('quota_identity')
            if not isinstance(agents,list) or any(a not in AGENTS for a in agents):raise ValueError('quota_agents')
            if any(q.get(k) is not None and not number(q[k]) for k in ('used_pct','remaining','limit','used','reset_ms','as_of_ms')):raise ValueError('quota_value')
            if q.get('used_pct') is not None and q['used_pct']>100:raise ValueError('quota_percent')
            if q.get('availability') not in ('available','unavailable','needs_auth','error'):raise ValueError('quota_availability')
            if not label(q.get('label',''),48) or not label(q.get('reason',''),64) or q.get('unit') not in ('percent','token','credit','request','USD','usd_micros','unlimited'):raise ValueError('quota_units')
            key=(provider,account,window)
            if key in seen:continue
            seen.add(key)
            out.append({k:q.get(k) for k in ('provider','account_hash','window_key','label','used_pct','remaining','limit','used','unit','reset_ms','as_of_ms','availability','reason','agents')})
        return out
    def query(self,subject='all',period='today'):
        if subject not in (*AGENTS,'all') or period not in PERIODS:raise ValueError('usage_query')
        today=datetime.datetime.now().astimezone().date();start=today-datetime.timedelta(days=PERIODS[period]-1)
        where='date>=? AND date<=?';params=[start.isoformat(),today.isoformat()]
        if subject!='all':where+=' AND agent=?';params.append(subject)
        rows=[dict(json.loads(r[0]),_as_of_ms=r[1]) for r in self.db.execute('SELECT data,asof FROM days WHERE '+where,params)]
        def summary(rs):
            costs={}
            for r in rs:
                c=r.get('actual_cost')
                if c:costs[c['currency']]=costs.get(c['currency'],0)+c['amount']
            return dict(**{k:summed(rs,k) for k in FIELDS},as_of_ms=min((r['_as_of_ms'] for r in rs),default=None),actual_costs=[{'currency':c,'amount':v,'coverage':'partial' if any(r.get('actual_cost') is None or r['coverage']['cost']!='complete' for r in rs) else 'complete','provenance':sorted({r['actual_cost']['provenance'] for r in rs if r.get('actual_cost') and r['actual_cost']['currency']==c})} for c,v in sorted(costs.items())],coverage='complete' if rs and all(r['coverage']['tokens']=='complete' for r in rs) else 'partial' if rs else 'unknown')
        history=[]
        for i in range(PERIODS[period]):
            d=(start+datetime.timedelta(days=i)).isoformat();history.append(dict(date=d,**summary([r for r in rows if r['date']==d])))
        models=[]
        for model in sorted({r['model'] for r in rows}):models.append(dict(model=model,**summary([r for r in rows if r['model']==model])))
        models.sort(key=lambda m:-(m['total_tokens'] or 0))
        agents=[dict(id=a,**summary([r for r in rows if r['agent']==a])) for a in AGENTS if subject in ('all',a)]
        quotas=[q for q in self.meta('quotas',[]) if subject=='all' or subject in q['agents']]
        asof=min((r['_as_of_ms'] for r in rows),default=None);age=now()-asof if asof else None
        return dict(version=1,revision=self.meta('revision',0),subject=subject,period=period,timezone=self.meta('timezone',str(datetime.datetime.now().astimezone().tzinfo)),start_date=start.isoformat(),end_date=today.isoformat(),as_of_ms=asof,stale=asof is not None and age>600000,status=self.status,error=self.error,collecting=self.collecting,summary=summary(rows),agents=agents,history=history,models=models[:100],model_count=len(models),quotas=quotas,sources=self.meta('sources',[]))

class UsageCollector:
    def __init__(self,store,project):
        self.store=store;self.project=Path(project);self.proc=None;self.request=asyncio.Event();self.stopping=False
    def refresh(self):self.request.set()
    async def terminate(self):
        proc=self.proc
        if proc and proc.returncode is None:
            try:os.killpg(proc.pid,signal.SIGTERM)
            except ProcessLookupError:pass
            try:await asyncio.wait_for(proc.wait(),5)
            except asyncio.TimeoutError:
                try:os.killpg(proc.pid,signal.SIGKILL)
                except ProcessLookupError:pass
                await proc.wait()
        self.proc=None
    async def run(self):
        entry=self.project/'usage-collector'/'index.js'
        while not self.stopping:
            reader=wake=None
            try:
                if not entry.exists():raise OSError('collector_not_installed')
                node=os.environ.get('BOT_NODE') or shutil.which('node') or '/opt/homebrew/bin/node'
                self.store.collecting=True
                self.proc=await asyncio.create_subprocess_exec(node,str(entry),'--watch','--root',str(self.store.root/'collector'),'--days','30',stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.DEVNULL,limit=16*1024*1024,start_new_session=True)
                reader=asyncio.create_task(self.proc.stdout.readline())
                while not self.stopping:
                    wake=asyncio.create_task(self.request.wait())
                    done,_=await asyncio.wait((reader,wake),timeout=420,return_when=asyncio.FIRST_COMPLETED)
                    if not done:raise ValueError('collector_timeout')
                    if wake in done:
                        self.request.clear();self.store.collecting=True
                        self.proc.stdin.write(b'{"action":"refresh"}\n');await self.proc.stdin.drain()
                    else:wake.cancel();await asyncio.gather(wake,return_exceptions=True)
                    if reader in done:
                        raw=reader.result()
                        if not raw or len(raw)>16*1024*1024:raise ValueError('collector_output')
                        self.store.ingest(json.loads(raw));self.store.collecting=False
                        reader=asyncio.create_task(self.proc.stdout.readline())
            except (OSError,ValueError,TypeError,KeyError,OverflowError,sqlite3.Error,asyncio.TimeoutError):
                self.store.status='error';self.store.error='collector_read_failed'
            finally:
                for task in (reader,wake):
                    if task and not task.done():task.cancel()
                await asyncio.gather(*(t for t in (reader,wake) if t),return_exceptions=True)
                await self.terminate();self.store.collecting=False
            if not self.stopping:await asyncio.sleep(15)
    async def close(self):
        self.stopping=True;self.request.set();await self.terminate()
