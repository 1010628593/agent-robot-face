"""Allowlisted source lifecycle readers. Cursors and identity context survive restarts."""
from __future__ import annotations
import datetime,json,sqlite3,time,re
from contextlib import closing
from pathlib import Path
from .runtime import now,ident

def timestamp(v):
    if isinstance(v,(int,float)):return int(v*1000 if v<1e11 else v)
    if isinstance(v,str):
        try:return int(datetime.datetime.fromisoformat(v.replace('Z','+00:00')).timestamp()*1000)
        except ValueError:pass
    return now()

def native_event(agent,d):
    """One-way privacy boundary: return metadata only, never raw payload persistence."""
    ex=d.get('extra') or {};name=d.get('hook_event_name') or d.get('event') or d.get('hook_event')
    sid=d.get('session_id') or d.get('conversation_id');rid=ex.get('turn_id') or d.get('generation_id') or d.get('turn_id')
    if agent=='hermes' and ex.get('platform',d.get('platform','cli'))!='cli':return None
    if d.get('parent_session_id'):return None
    kinds={'beforeSubmitPrompt':'start','UserPromptSubmit':'start','pre_llm_call':'start','preToolUse':'tool_start','PreToolUse':'tool_start','pre_tool_call':'tool_start','postToolUse':'tool_end','PostToolUse':'tool_end','post_tool_call':'tool_end','PermissionRequest':'waiting'}
    kind=kinds.get(name)
    if name=='stop':kind={'completed':'done','aborted':'cancelled','error':'error'}.get(d.get('status'))
    if name=='on_session_end':kind='cancelled' if ex.get('interrupted') is True else 'error' if ex.get('failed') is True else 'done' if ex.get('completed') is True else 'unknown'
    # WorkBuddy Stop contract is still unverified. Its candidate hook remains diagnostic only.
    if agent=='workbuddy':return None
    if not sid or not rid or not kind:return None
    return dict(agent=agent,session=sid,run=rid,kind=kind,ts=timestamp(d.get('timestamp')),tool_name=d.get('tool_name',''),tool_id=ex.get('tool_call_id') or d.get('tool_use_id') or d.get('tool_call_id',''),channel=agent+'_hook',reason='approval' if name=='PermissionRequest' else 'input' if kind=='waiting' else 'none')

class Adapters:
    def __init__(self,ledger,home=None):self.l=ledger;self.home=Path(home or Path.home());self.last_discovery=0;self.files=[];self.rotation=0;self.deadline=0
    def discover(self):
        h=self.home;files=[];cut=time.time()-86400*2
        for days in range(3):
            day=datetime.datetime.now()-datetime.timedelta(days=days)
            base=h/'.codex/sessions'/day.strftime('%Y/%m/%d')
            if base.exists():files.extend(('codex',p) for p in base.glob('*.jsonl'))
        base=h/'.workbuddy/projects'
        if base.exists():files.extend(('workbuddy',p) for p in base.glob('*/*.jsonl') if p.stat().st_mtime>cut)
        base=h/'Library/Application Support/Cursor/logs'
        if base.exists():
            for day in range(3):
                pattern=(datetime.datetime.now()-datetime.timedelta(days=day)).strftime('%Y%m%d')+'*/window*/output*/cursor.hooks*.log'
                files.extend(('cursor',p) for p in base.glob(pattern))
        self.files=sorted(files,key=lambda v:v[1].stat().st_mtime,reverse=True)[:200]
        p=self.l.root/'hooks.jsonl'
        if p.exists():self.files.insert(0,('hook',p))
        self.last_discovery=time.monotonic()
    def poll(self):
        started=time.monotonic()
        self.deadline=started+.04
        if time.monotonic()-self.last_discovery>10:self.discover()
        # Reserve half the bounded work window for live streams. Historical
        # files must not delay a new hook or an active desktop turn for minutes.
        for a,p in self.files:
            if time.monotonic()>self.deadline:break
            try:
                st=p.stat();c=self.l.cursor(str(p))
                if a!='hook' and st.st_mtime<time.time()-120:continue
                if c.get('offset',0)>=st.st_size:continue
                self.visit(a,p,1000)
            except OSError:
                if a!='hook':self.l.scan(a,'reader_error')
        self.deadline=started+.08
        for _ in range(min(12,len(self.files))):
            if time.monotonic()>self.deadline:break
            a,p=self.files[self.rotation%len(self.files)];self.rotation+=1
            self.visit(a,p)
        if time.monotonic()<self.deadline:
            try:self.hermes()
            except (OSError,ValueError,sqlite3.Error):self.l.scan('hermes','reader_error')
        backlog={};sessions={}
        for a,p in self.files:
            if a=='hook':continue
            try:
                st=p.stat();c=self.l.cursor(str(p))
                if st.st_mtime<time.time()-120 or c.get('child'):continue
                pending=max(0,st.st_size-c.get('offset',0))
                if pending>65536:
                    backlog[a]=backlog.get(a,0)+pending
                    sid=c.get('session') if a=='codex' else p.stem if a=='workbuddy' else None
                    if sid:sessions.setdefault(a,set()).add(ident(sid))
            except OSError:pass
        self.l.synchronizing=backlog;self.l.syncing_sessions=sessions
    def visit(self,a,p,max_records=100):
        try:
            if a=='cursor':self.cursor_log(p)
            else:self.read(a,p,max_records)
            if a!='hook':self.l.scan(a)
        except (OSError,ValueError,TypeError,KeyError,sqlite3.Error):
            if a!='hook':self.l.scan(a,'reader_error')
    def ingest_hook(self,d,retry=False):
        if d.get('agent')=='hermes':
            key='hermes_turn_'+ident(str(d.get('session'))+'|'+str(d.get('run')))
            rid=self.l.get(key)
            if rid is None and d.get('kind')=='start':
                path=self.home/'.hermes/state.db'
                if path.exists():
                    with closing(sqlite3.connect(f'file:{path}?mode=ro',uri=True,timeout=.05)) as db:
                        rows=db.execute("SELECT m.id,m.timestamp FROM messages m JOIN sessions s ON s.id=m.session_id WHERE s.id=? AND s.source='cli' AND s.parent_session_id IS NULL AND m.role='user' ORDER BY m.timestamp DESC LIMIT 3",(d['session'],)).fetchall()
                    # First CLI turn is unambiguous. Multi-turn correlation requires exactly one nearby
                    # human row; pre_llm hooks may precede its INSERT. Ambiguity stays pending, never guesses.
                    candidates=rows if len(rows)==1 else [r for r in rows if abs(r[1]*1000-d['ts'])<=5000]
                    if len(candidates)==1:
                        rid=str(candidates[0][0]);self.l.put(key,rid)
            if rid is None:
                if not retry:
                    pending=self.l.get('pending_hermes_hooks',[])
                    if d not in pending:pending.append(d)
                    self.l.put('pending_hermes_hooks',pending[-256:])
                self.l.scan('hermes','hook_turn_correlation_pending');return False
            d=dict(d,run=rid)
        if set(d)<= {'agent','session','run','kind','ts','tool_name','tool_id','channel','reason'}:self.l.emit(**d)
        return True
    def cursor_log(self,p):
        c=self.l.cursor(str(p));st=p.stat();offset=c.get('offset',0)
        if c.get('inode')!=st.st_ino or offset>st.st_size:offset=0
        with p.open('rb') as f:f.seek(offset);raw=f.read(262144)
        text=raw.decode('utf-8',errors='replace');pos=0;decoder=json.JSONDecoder();consumed=0
        for _ in range(30):
            if time.monotonic()>self.deadline:break
            marker=text.find('INPUT:\n',pos)
            if marker<0:
                consumed=max(consumed,len(text)-16);break
            start=marker+7
            try:d,end=decoder.raw_decode(text[start:].lstrip());end+=start+len(text[start:])-len(text[start:].lstrip())
            except ValueError:
                consumed=max(consumed,marker);break
            stamps=re.findall(r'\[(\d{4}-\d{2}-\d{2}T[^]]+)\]',text[pos:marker])
            if stamps:c['timestamp']=timestamp(stamps[-1])
            d['timestamp']=c.get('timestamp',int(st.st_mtime*1000))
            e=native_event('cursor',d)
            if e:self.ingest_hook(e)
            pos=end;consumed=end
        self.l.set_cursor(str(p),dict(offset=offset+len(text[:consumed].encode('utf-8')),inode=st.st_ino,timestamp=c.get('timestamp')))
    def read(self,a,p,max_records=100):
        c=self.l.cursor(str(p));st=p.stat()
        if c.get('inode')!=st.st_ino or c.get('offset',0)>st.st_size:c={}
        offset=c.get('offset',0)
        with p.open('rb') as f:
            f.seek(offset)
            for _ in range(max_records):
                if time.monotonic()>self.deadline:break
                start=f.tell();line=f.readline(2*1024*1024+1)
                if not line:break
                if c.get('discard_oversized'):
                    c['discard_oversized']=not line.endswith(b'\n')
                    continue
                if len(line)>2*1024*1024:
                    if a!='hook':self.l.put('oversized_record_'+a,True)
                    c['discard_oversized']=not line.endswith(b'\n')
                    continue
                if not line.endswith(b'\n'):f.seek(start);break
                try:d=json.loads(line)
                except (ValueError,UnicodeDecodeError):continue
                if a=='hook':
                    # hook script has already sanitized, strict keys still enforced on replay.
                    self.ingest_hook(d)
                elif a=='codex':self.codex(d,c)
                else:self.workbuddy(d,c,p.stem)
            c.update(offset=f.tell(),inode=st.st_ino)
        self.l.set_cursor(str(p),c)
    def codex(self,d,c):
        p=d.get('payload') or {}; typ=d.get('type');ts=timestamp(d.get('timestamp'))
        if typ=='session_meta':
            src=p.get('source');c['session']=p.get('id');c['child']=isinstance(src,dict) or bool(p.get('parent_thread_id'));c['entry']='cli' if src=='exec' else 'desktop' if src=='vscode' and p.get('originator')=='Codex Desktop' else 'unsupported';return
        if c.get('child') or c.get('entry')=='unsupported':return
        if typ=='turn_context' and p.get('turn_id'):c['run']=p['turn_id']
        if typ=='event_msg':
            t=p.get('type');rid=p.get('turn_id') or c.get('run')
            if t=='task_started':c['run']=rid
            kind={'task_started':'start','task_complete':'error' if p.get('error') is not None else 'done','turn_aborted':'cancelled' if p.get('reason') in ('interrupted','user_cancelled','user_canceled') else 'unknown'}.get(t)
            if kind:self.l.emit('codex',c.get('session'),rid,kind,ts,channel='codex_'+c.get('entry','unknown'))
            if t in ('approval_requested','user_input_requested'):
                self.l.emit('codex',c.get('session'),rid,'waiting',ts,tool_id=p.get('call_id',''),channel='codex_'+c.get('entry','unknown'),reason='approval' if t=='approval_requested' else 'input')
            if t=='token_count':
                info=p.get('info') or {};u=info.get('total_token_usage') or {}
                for key in ('input_tokens','output_tokens','total_tokens'):self.l.record_usage('codex',c.get('session'),key,u.get(key),'exact',ts)
        if typ=='response_item' and p.get('type') in ('function_call','custom_tool_call','function_call_output','custom_tool_call_output'):
            end=p['type'].endswith('output');self.l.emit('codex',c.get('session'),c.get('run'),'tool_end' if end else 'tool_start',ts,p.get('name',''),p.get('call_id',''),channel='codex_'+c.get('entry','unknown'))
    def workbuddy(self,d,c,sid):
        typ=d.get('type');role=d.get('role');pd=d.get('providerData') or {};eid=d.get('id');parent=d.get('parentId');ts=timestamp(d.get('timestamp'));nodes=c.setdefault('nodes',{})
        if typ=='message' and role=='user':
            if pd.get('isMeta') is True:nodes[eid]=None;return
            nodes[eid]=eid;c['run']=eid;self.l.emit('workbuddy',sid,eid,'start',ts,channel='workbuddy_desktop_jsonl');return
        rid=nodes.get(parent)
        if eid:nodes[eid]=rid
        if len(nodes)>3000:
            for key in list(nodes)[:1000]:del nodes[key]
        if not rid:return
        kind=None
        if typ=='function_call':kind='tool_start'
        elif typ=='function_call_result':kind='tool_end'
        elif typ=='message' and role=='assistant':
            if d.get('status')=='completed' and not pd.get('error'):kind='done'
            elif d.get('status')=='incomplete':kind='unknown'
        if kind:self.l.emit('workbuddy',sid,rid,kind,ts,d.get('name',''),d.get('callId',''),channel='workbuddy_desktop_jsonl')
    def hermes(self):
        path=self.home/'.hermes/state.db'
        if not path.exists():self.l.scan('hermes','not_installed');return
        c=self.l.cursor('hermes_sql');last=c.get('id',0)
        with closing(sqlite3.connect(f'file:{path}?mode=ro',uri=True,timeout=.2)) as db:
            db.row_factory=sqlite3.Row
            # Exclude gateway/ACP/subagents at SQL boundary. Never SELECT content or tool arguments.
            rows=db.execute('''SELECT m.id,m.session_id,m.role,m.tool_call_id,m.tool_name,m.timestamp,m.finish_reason FROM messages m JOIN sessions s ON s.id=m.session_id WHERE m.id>? AND s.source='cli' AND s.parent_session_id IS NULL AND m.timestamp>? ORDER BY m.id LIMIT 100''',(last,time.time()-86400*2)).fetchall()
            runs=c.get('runs',{})
            for r in rows:
                sid=r['session_id'];ts=timestamp(r['timestamp']);rid=runs.get(sid)
                if r['role']=='user':rid=str(r['id']);runs[sid]=rid;self.l.emit('hermes',sid,rid,'start',ts,channel='hermes_cli_db')
                elif r['role']=='tool' and rid:
                    # Tool result proves an executed call but not its original start time.
                    self.l.emit('hermes',sid,rid,'tool_start',ts,r['tool_name'],r['tool_call_id'],channel='hermes_cli_db')
                    self.l.emit('hermes',sid,rid,'tool_end',ts,r['tool_name'],r['tool_call_id'],channel='hermes_cli_db')
                elif r['role']=='assistant' and r['finish_reason']=='stop' and rid:self.l.emit('hermes',sid,rid,'done',ts,channel='hermes_cli_db')
                last=r['id']
            # Cost units converted exactly once; unavailable cost remains null.
            for r in db.execute("SELECT id,input_tokens,output_tokens,actual_cost_usd,estimated_cost_usd,cost_status,last_activity_at,ended_at,started_at FROM sessions WHERE source='cli' AND parent_session_id IS NULL AND started_at>?",(time.time()-86400,)):
                ts=timestamp(r['last_activity_at'] or r['ended_at'] or r['started_at'])
                if r['input_tokens'] or r['output_tokens']:self.l.record_usage('hermes',r['id'],'total_tokens',(r['input_tokens'] or 0)+(r['output_tokens'] or 0),'exact',ts)
                val=r['actual_cost_usd'] if r['actual_cost_usd'] is not None else r['estimated_cost_usd']
                if val is not None:self.l.record_usage('hermes',r['id'],'cost_usd_micros',round(val*1e6),'exact' if r['actual_cost_usd'] is not None else 'estimated',ts)
        self.l.set_cursor('hermes_sql',{'id':last,'runs':runs})
        pending=self.l.get('pending_hermes_hooks',[]);remaining=[]
        for event in pending:
            if not self.ingest_hook(event,retry=True):remaining.append(event)
        if pending:self.l.put('pending_hermes_hooks',remaining)
        self.l.scan('hermes','hook_turn_correlation_pending' if remaining else None)
