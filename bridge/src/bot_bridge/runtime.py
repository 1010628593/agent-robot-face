"""Private metadata ledger and authoritative selection projection. No raw source payloads."""
from __future__ import annotations
import hashlib,json,sqlite3,time,datetime,os
from pathlib import Path
AGENTS=('codex','cursor','hermes','workbuddy')
ACTIVE=('working','tool','waiting')
TERMINAL=('done','error','cancelled','unknown')
def now(): return int(time.time()*1000)
def ident(value): return hashlib.sha256(str(value).encode()).hexdigest()[:32]
def tool(value):
    v=str(value or '').lower()
    return next((name for keys,name in [(('shell','bash','terminal','exec_command'),'terminal'),(('read','search','grep','find'),'read'),(('write','edit','patch'),'edit'),(('web','browse','fetch'),'web'),(('agent','task'),'agent')] if any(k in v for k in keys)),'tool')

class Ledger:
    def __init__(self,root):
        self.root=Path(root); self.root.mkdir(parents=True,exist_ok=True); os.chmod(self.root,0o700)
        self.db=sqlite3.connect(self.root/'ledger.sqlite3'); self.db.row_factory=sqlite3.Row
        self.db.executescript('''PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY,agent TEXT,session TEXT,run TEXT,kind TEXT,ts INTEGER,tool TEXT,tool_id TEXT,channel TEXT);
CREATE TABLE IF NOT EXISTS runs(agent TEXT,session TEXT,run TEXT,state TEXT,started INTEGER,updated INTEGER,tool TEXT,terminal INTEGER DEFAULT 0,PRIMARY KEY(agent,session,run));
CREATE TABLE IF NOT EXISTS cursors(key TEXT PRIMARY KEY,value TEXT);
CREATE TABLE IF NOT EXISTS source(agent TEXT PRIMARY KEY,observed INTEGER,last_event INTEGER,last_scan INTEGER,error TEXT);
CREATE TABLE IF NOT EXISTS usage(agent TEXT,session TEXT,key TEXT,value REAL,quality TEXT,ts INTEGER,PRIMARY KEY(agent,session,key));
CREATE TABLE IF NOT EXISTS attention(agent TEXT,session TEXT,run TEXT,kind TEXT,pending INTEGER,PRIMARY KEY(agent,session,run));
CREATE TABLE IF NOT EXISTS tools(agent TEXT,session TEXT,run TEXT,tool_id TEXT,state TEXT,updated INTEGER,PRIMARY KEY(agent,session,run,tool_id));
CREATE TABLE IF NOT EXISTS usage_samples(agent TEXT,session TEXT,key TEXT,day TEXT,value REAL,quality TEXT,ts INTEGER,PRIMARY KEY(agent,session,key,day));
CREATE TABLE IF NOT EXISTS kv(key TEXT PRIMARY KEY,value TEXT);
''')
        self.synchronizing={};self.syncing_sessions={}
        self.started=now(); self.selection=self.get('selection',{'mode':'auto','selected_agent':'codex','selection_rev':1})
        self.paused=self.get('paused',False)
    def get(self,key,default=None):
        r=self.db.execute('SELECT value FROM kv WHERE key=?',(key,)).fetchone(); return json.loads(r[0]) if r else default
    def put(self,key,value):
        self.db.execute('INSERT OR REPLACE INTO kv VALUES(?,?)',(key,json.dumps(value)));self.db.commit()
    def cursor(self,key):
        r=self.db.execute('SELECT value FROM cursors WHERE key=?',(ident(key),)).fetchone();return json.loads(r[0]) if r else {}
    def set_cursor(self,key,val):
        self.db.execute('INSERT OR REPLACE INTO cursors VALUES(?,?)',(ident(key),json.dumps(val))); self.db.commit()
    def scan(self,agent,error=None):
        self.db.execute('INSERT INTO source VALUES(?,0,NULL,?,?) ON CONFLICT(agent) DO UPDATE SET last_scan=excluded.last_scan,error=excluded.error',(agent,now(),error));self.db.commit()
    def emit(self,agent,session,run,kind,ts=None,tool_name='',tool_id='',channel='native',event_id=None,reason='none'):
        if agent not in AGENTS or kind not in ('start','tool_start','tool_end','waiting','done','error','cancelled','unknown'): return
        if not session or not run:return
        ts=ts or now(); session=ident(session);run=ident(run);tid=ident(tool_id) if tool_id else ''
        eid=ident(event_id or '|'.join((agent,session,run,kind,tid,str(ts) if kind=='waiting' else '')))
        # All adapters share this identity, so hook + fallback redelivery cannot double count.
        with self.db:
            if not self.db.execute('INSERT OR IGNORE INTO events VALUES(?,?,?,?,?,?,?,?,?)',(eid,agent,session,run,kind,ts,tool(tool_name) if tool_name else '',tid,channel)).rowcount:return
            self.db.execute('INSERT INTO source VALUES(?,1,?,?,NULL) ON CONFLICT(agent) DO UPDATE SET observed=1,last_event=MAX(COALESCE(last_event,0),excluded.last_event)',(agent,ts,now()))
            old=self.db.execute('SELECT * FROM runs WHERE agent=? AND session=? AND run=?',(agent,session,run)).fetchone()
            if old and old['terminal'] and old['state']!='unknown':return
            if old and ts<old['updated'] and kind not in ('tool_start','tool_end','waiting','start'):return
            if tid and kind in ('tool_start','tool_end','waiting'):
                prior=self.db.execute('SELECT state,updated FROM tools WHERE agent=? AND session=? AND run=? AND tool_id=?',(agent,session,run,tid)).fetchone()
                if prior is None or (prior['state']!='tool_end' and ts>=prior['updated']):
                    self.db.execute('INSERT OR REPLACE INTO tools VALUES(?,?,?,?,?,?)',(agent,session,run,tid,kind,ts))
            if kind=='waiting':self.db.execute('INSERT OR REPLACE INTO kv VALUES(?,?)',('reason_'+agent+'_'+run,json.dumps(reason if reason in ('input','approval') else 'input')))
            if kind=='start':self.db.execute('UPDATE attention SET pending=0 WHERE agent=? AND session=? AND run!=?',(agent,session,run))
            if kind in ('waiting','error') and ts>=self.started:
                self.db.execute('INSERT OR REPLACE INTO attention VALUES(?,?,?,?,1)',(agent,session,run,kind))
            if kind in ('done','cancelled'):self.db.execute("UPDATE attention SET pending=0 WHERE agent=? AND session=? AND run=? AND kind='waiting'",(agent,session,run))
            state={'start':'working','tool_start':'tool','tool_end':'working'}.get(kind,kind)
            if kind not in TERMINAL:
                tools=self.db.execute('SELECT state FROM tools WHERE agent=? AND session=? AND run=?',(agent,session,run)).fetchall()
                if any(t['state']=='waiting' for t in tools) or (old and old['state']=='waiting' and not tid and kind!='start'):state='waiting'
                elif any(t['state']=='tool_start' for t in tools):state='tool'
                if state!='waiting':self.db.execute("UPDATE attention SET pending=0 WHERE agent=? AND session=? AND run=? AND kind='waiting'",(agent,session,run))
            self.db.execute('INSERT OR REPLACE INTO runs VALUES(?,?,?,?,?,?,?,?)',(agent,session,run,state,min(old['started'],ts) if old else ts,max(old['updated'],ts) if old else ts,tool(tool_name) if kind=='tool_start' else '',int(kind in TERMINAL)))
    def record_usage(self,agent,session,key,value,quality,ts):
        if key not in ('input_tokens','output_tokens','total_tokens','cost_usd_micros','requests') or not isinstance(value,(int,float)) or isinstance(value,bool) or value<0:return
        day=datetime.datetime.fromtimestamp(ts/1000).astimezone().date().isoformat()
        old=self.db.execute('SELECT value,ts FROM usage WHERE agent=? AND session=? AND key=?',(agent,ident(session),key)).fetchone()
        if old and ts<old['ts']:return
        with self.db:
            self.db.execute('INSERT OR REPLACE INTO usage VALUES(?,?,?,?,?,?)',(agent,ident(session),key,value,quality,ts))
            # First cumulative sample establishes a baseline, never attributes lifetime usage to today.
            if old and value>=old['value'] and datetime.datetime.fromtimestamp(old['ts']/1000).astimezone().date().isoformat()==day:
                delta=value-old['value']
                self.db.execute('INSERT INTO usage_samples VALUES(?,?,?,?,?,?,?) ON CONFLICT(agent,session,key,day) DO UPDATE SET value=value+excluded.value,quality=excluded.quality,ts=excluded.ts',(agent,ident(session),key,day,delta,quality,ts))

    def latest(self,agent):
        # Only latest run in each session competes; stale errors from previous turns cannot steal focus.
        rows=self.db.execute('''SELECT r.* FROM runs r WHERE agent=? AND started=(SELECT MAX(x.started) FROM runs x WHERE x.agent=r.agent AND x.session=r.session) ORDER BY updated DESC,run''',(agent,)).fetchall()
        blocked=self.syncing_sessions.get(agent,set())
        rows=[r for r in rows if r['session'] not in blocked]
        if self.synchronizing.get(agent) and not blocked:rows=[]
        live=[r for r in rows if (r['state']=='error' and self.pending(agent,r['run'])) or (r['state']=='waiting' and self.pending(agent,r['run'])) or (r['state'] in ACTIVE and now()-r['updated']<120000)]
        selected=self.get('focus_'+agent)
        for states in (('waiting',),('error',),ACTIVE):
            candidates=[r for r in live if r['state'] in states]
            if candidates:
                hit=next((r for r in candidates if r['run']==selected),min(candidates,key=lambda r:(r['started'],r['run'])))
                if hit['run']!=selected:self.put('focus_'+agent,hit['run'])
                return dict(hit)
        return dict(rows[0]) if rows else {}
    def pending(self,agent,run):
        r=self.db.execute('SELECT pending FROM attention WHERE agent=? AND run=?',(agent,run)).fetchone();return bool(r and r[0])
    def auto(self):
        if self.selection['mode']!='auto':return
        focus={a:self.latest(a) for a in AGENTS}; chosen=None
        for state in ('waiting','error'):
            candidates=[a for a,r in focus.items() if r.get('state')==state and self.pending(a,r['run'])]
            if candidates:
                current=self.selection['selected_agent'];chosen=current if current in candidates else sorted(candidates,key=lambda a:(focus[a]['started'],AGENTS.index(a)))[0];break
        current=self.selection['selected_agent']
        if chosen is None and focus[current].get('state') in ACTIVE and now()-focus[current]['updated']<120000:chosen=current
        if chosen is None:
            candidates=[a for a,r in focus.items() if r.get('state') in ACTIVE and now()-r['updated']<120000]
            if candidates:chosen=sorted(candidates,key=lambda a:(-focus[a]['updated'],AGENTS.index(a)))[0]
        if chosen and chosen!=current:self.select('auto',chosen)
    def select(self,mode,agent):
        self.selection={'mode':mode,'selected_agent':agent,'selection_rev':self.selection['selection_rev']+1};self.put('selection',self.selection)
    def source(self,a):
        row=self.db.execute('SELECT * FROM source WHERE agent=?',(a,)).fetchone();return dict(row) if row else {'observed':0,'last_event':None,'last_scan':None,'error':None}
    def active_count(self,a):
        rows=self.db.execute("SELECT distinct session FROM runs WHERE agent=? AND state IN ('working','tool','waiting') AND updated>? AND NOT EXISTS(SELECT 1 FROM runs newer WHERE newer.agent=runs.agent AND newer.session=runs.session AND newer.started>runs.started)",(a,now()-120000)).fetchall()
        return sum(r[0] not in self.syncing_sessions.get(a,set()) for r in rows) if not (self.synchronizing.get(a) and not self.syncing_sessions.get(a)) else 0
    def focus(self):
        a=self.selection['selected_agent'];r=self.latest(a);age=max(0,now()-r['updated']) if r else None;state=r.get('state','unknown')
        if self.synchronizing.get(a) and not r:
            return dict(agent_id=a,selection_rev=self.selection['selection_rev'],session_key=None,run_id=None,state='unknown',reason='unobserved',quality='inferred',source_age_ms=None,stale=True,tool='',detail='syncing_history',run_elapsed_ms=0,active_sessions=0,progress=None)
        return dict(agent_id=a,selection_rev=self.selection['selection_rev'],session_key=r.get('session'),run_id=r.get('run'),state=state,reason={'waiting':self.get('reason_'+a+'_'+r.get('run',''),'input'),'done':'completed','error':'failed','cancelled':'cancelled','unknown':'unobserved'}.get(state,'none'),quality='observed' if r else 'inferred',source_age_ms=age,stale=state in ACTIVE and age is not None and age>120000,tool=r.get('tool',''),detail='',run_elapsed_ms=max(0,(r.get('updated',now()) if r.get('terminal') else now())-r.get('started',now())),active_sessions=min(16,self.active_count(a)),progress=None)
    def stats(self):
        a=self.selection['selected_agent']; t=datetime.datetime.now().astimezone();start=int(t.replace(hour=0,minute=0,second=0,microsecond=0).timestamp()*1000);end=int((t.replace(hour=0,minute=0,second=0,microsecond=0)+datetime.timedelta(days=1)).timestamp()*1000)
        s=self.source(a);metrics=[]
        for key,kind,unit,label in [('turns','start','turn','Turns'),('tool_calls','tool_start','call','Tools')]:
            val=self.db.execute('SELECT count(*) FROM events WHERE agent=? AND kind=? AND ts>=? AND ts<?',(a,kind,start,end)).fetchone()[0] if s['observed'] else None
            metrics.append(dict(key=key,label=label,value=val,unit=unit,quality='exact' if val is not None else 'unavailable',coverage='partial' if val is not None else 'unknown',source=a+'_ledger',as_of_ms=s['last_event'] if val is not None else None,stale_after_ms=120000))
        for key,unit,label in [('total_tokens','token','Tokens'),('cost_usd_micros','usd_micros','Cost')]:
            rows=self.db.execute('SELECT sum(value),max(ts),min(quality) FROM usage_samples WHERE agent=? AND key=? AND ts>=? AND ts<?',(a,key,start,end)).fetchone();val=rows[0]
            metrics.append(dict(key=key,label=label,value=val,unit=unit,quality=rows[2] if val is not None else 'unavailable',coverage='partial' if val is not None else 'unknown',source=a+'_native_usage',as_of_ms=rows[1],stale_after_ms=120000))
        return dict(agent_id=a,selection_rev=self.selection['selection_rev'],sent_at_ms=now(),scope=dict(kind='today',timezone=os.environ.get('TZ','Asia/Shanghai'),start_ms=start,end_ms=end),metrics=metrics,quotas=[],sparkline=[])
    def catalog(self):
        out=[]
        for a in AGENTS:
            s=self.source(a);r=self.latest(a);sync=bool(self.synchronizing.get(a)) and not r;out.append(dict(id=a,label={'codex':'Codex','cursor':'Cursor','hermes':'Hermes','workbuddy':'WorkBuddy'}[a],state=r.get('state','unknown'),health='partial' if s['observed'] else 'unavailable',active_sessions=0 if sync else min(16,self.active_count(a)),attention_count=int(r.get('state') in ('waiting','error') and self.pending(a,r.get('run',''))),capabilities=dict(state='observed' if s['observed'] and not sync else 'none',usage='automatic' if s['observed'] else 'none',quota='none',open_agent=False,open_usage=False)))
        return {'agents':out}
