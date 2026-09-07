"""Protocol v3 serial link. Exactly one coroutine owns all serial reads/writes."""
import asyncio,json,secrets,time,re
import serial
from serial.tools import list_ports
from .models import strict_load
from .runtime import now,AGENTS
from .usage_wire import project
HEX=re.compile(r'^[0-9a-f]{32}$')
class USB:
    def __init__(self,ledger,port=None):
        self.l=ledger;self.requested_port=port;self.serial=None;self.link=None;self.seq=0;self.rxseq=0;self.pending=[];self.actions={};self.buffer=bytearray();self.tx_buffer=bytearray();self.last_rx=0;self.last_ping=0;self.last_projection=0;self.last_rev=0;self.stopping=False
        self.usage=None;self.usage_view=dict(usage_rev=1,subject="current",period="today",page=0);self.usage_actions={}
        self.state=dict(connected=False,port=None,firmware=None,protocol_version=3,last_seen_ms=None,error=None,diagnostics=None)
    def close(self):
        if self.serial:self.serial.close()
        self.serial=None;self.link=None;self.buffer.clear();self.tx_buffer.clear();self.pending.clear();self.state['connected']=False;self.state['port']=None
    def send(self,typ,body):
        self.seq+=1;d=dict(v=3,type=typ,link_id=self.link,seq=self.seq,body=body);raw=json.dumps(d,separators=(',',':'),allow_nan=False).encode()
        if len(raw)>8192:raise ValueError('frame_limit')
        self.state['last_tx_type']=typ;self.state['last_tx_bytes']=len(raw)+6
        framed=b'@bot '+raw+b'\n'
        if len(self.tx_buffer)+len(framed)>32768:raise OSError('tx_backpressure')
        self.tx_buffer.extend(framed)
    def flush_tx(self):
        if not self.tx_buffer:return
        # The UI consumes at most 8 x 256 bytes per tick and may spend >100ms
        # rendering. Pace bursts so its 8KiB RX queue survives those pauses.
        try:n=self.serial.write(bytes(self.tx_buffer[:512]))
        except (BlockingIOError,serial.SerialTimeoutException):return
        if n:del self.tx_buffer[:n]
        self.state['tx_pending_bytes']=len(self.tx_buffer)
    def snapshots(self):
        self.send('catalog',self.l.catalog());self.send('focus',self.l.focus());self.send('stats',self.l.stats())
        if self.usage:self.send('usage',project(self.usage,self.usage_view,self.l.selection['selected_agent']))
        self.last_projection=time.monotonic()
    def receive(self,line):
        if line.startswith(b'@link '):
            try:
                d=strict_load(line[6:])
                if isinstance(d,dict) and set(d) in ({'rx_dropped','bad_lines','queue_restarts','timeout_restarts','tx_failed','queue_depth'},{'rx_dropped','bad_lines','queue_restarts','timeout_restarts','tx_failed','rx_pending_bytes','rx_chunks','rx_bytes','poll_gap_max_ms'}) and all(type(v) is int and 0<=v<=4294967295 for v in d.values()):self.state['link_diagnostics']=dict(d,as_of_ms=now())
            except (ValueError,UnicodeDecodeError):pass
            return
        if line.startswith(b'@heap '):
            try:
                d=strict_load(line[6:])
                keys={'uptime_ms','internal_free','largest_free','minimum_free','psram_free','screen'}
                if isinstance(d,dict) and set(d)==keys and all(type(v) is int and 0<=v<=4294967295 for v in d.values()):
                    self.state['heap_diagnostics']=dict(d,as_of_ms=now())
            except (ValueError,UnicodeDecodeError):pass
            return
        if line.startswith(b'@touch '):
            try:
                d=strict_load(line[7:])
                keys={'reads','dual','errors','overflow','raw_count','wire_count','record0','record1','count','gs','pet','blocked','edge','x','y','taps','expression','motion_count','tapped','tx','ty','left','right','revision'}
                if isinstance(d,dict) and set(d)==keys and all(type(v) in (int,float) and abs(v)<=4294967295 for v in d.values()):
                    self.state['touch_diagnostics']=dict(d,as_of_ms=now())
            except (ValueError,UnicodeDecodeError):pass
            return
        if line.startswith(b'@diag '):
            try:
                d=strict_load(line[6:]);keys={'render_updates','window_ms','avg_us','max_us'}
                if not isinstance(d,dict) or set(d) not in (keys,keys|{'projection'},keys|{'usage'},keys|{'projection','usage'}):return
                if not all(type(d[k]) is int and 0<=d[k]<=2147483647 for k in keys):return
                if 'projection' in d:
                    p=d['projection']
                    if not isinstance(p,dict) or set(p)!={'agent_id','mode','selection_rev','focus_selection_rev','state','run_id','screen'}:return
                    if p['agent_id'] not in AGENTS or p['mode'] not in ('auto','pinned') or p['screen'] not in ('face','picker','stats'):return
                    if p['state'] not in ('idle','working','tool','waiting','done','error','cancelled','unknown'):return
                    if type(p['selection_rev']) is not int or not 0<=p['selection_rev']<=2147483647:return
                    if p['focus_selection_rev'] is not None and (type(p['focus_selection_rev']) is not int or not 0<=p['focus_selection_rev']<=2147483647):return
                    if p['run_id'] is not None and (not isinstance(p['run_id'],str) or not HEX.fullmatch(p['run_id'])):return
                if 'usage' in d:
                    u=d['usage']
                    if not isinstance(u,dict) or set(u)!={'usage_rev','data_rev','subject','period','page','level'}:return
                    if u['subject'] not in (*AGENTS,'all','current') or u['period'] not in ('today','7d','30d'):return
                    if any(type(u[k]) is not int or not 0<=u[k]<=2147483647 for k in ('usage_rev','data_rev','page','level')) or u['page']>33 or u['level']>2:return
                self.state['diagnostics']=dict(d,as_of_ms=now())
            except (ValueError,UnicodeDecodeError):pass
            return
        if not line.startswith(b'@bot '):
            # Keep only bounded device allocation/panic diagnostics, never protocol payloads.
            clean=re.sub(rb'\x1b\[[0-9;]*m',b'',line[:1024]).decode('ascii',errors='replace')
            if any(marker in clean.lower() for marker in ('abort() was called','out of memory','alloc failed','malloc failed','assert failed','guru meditation','backtrace:','heap_caps','failed to allocate','draw_buf_malloc','lv_malloc','lv_draw_layer')):
                recent=self.state.setdefault('firmware_errors',[])
                recent.append(dict(as_of_ms=now(),message=clean[:384]))
                del recent[:-20]
            return
        try:d=strict_load(line[5:])
        except (ValueError,UnicodeDecodeError):
            self.state['invalid_protocol_json']=self.state.get('invalid_protocol_json',0)+1;return
        if isinstance(d,dict) and d.get('type')=='hello' and type(d.get('v')) is int and d['v']!=3:
            self.state['error']='protocol_mismatch';return
        if not isinstance(d,dict) or set(d)!={'v','type','link_id','seq','body'} or type(d['v']) is not int or d['v']!=3 or type(d['seq']) is not int or not 0<=d['seq']<=2147483647 or not isinstance(d['body'],dict):return
        b=d['body'];typ=d['type']
        counters=self.state.setdefault('protocol_rx',dict(hello=0,pong=0,accepted_pong=0,link_rejected=0,seq_rejected=0))
        if typ in ('hello','pong'):counters[typ]+=1
        if typ=='hello':
            if d['link_id'] is not None or d['seq']!=0 or set(b)!={'device_id','boot_id','firmware','display','min_version','max_version','handshake_id'}:return
            if any(not isinstance(b[k],str) or not HEX.fullmatch(b[k]) for k in ('boot_id','handshake_id')):return
            if type(b['min_version']) is not int or type(b['max_version']) is not int or not b['min_version']<=3<=b['max_version']:self.state['error']='protocol_mismatch';return
            if not isinstance(b['firmware'],str) or not 1<=len(b['firmware'])<=32 or not b['firmware'].isascii():return
            if not isinstance(b['device_id'],str) or len(b['device_id'])>64 or not b['device_id'].isascii() or b['display']!={'width':466,'height':466}:return
            self.state['hello_pending_tx_bytes']=len(self.tx_buffer)
            self.usage_actions={};self.usage_view=dict(usage_rev=1,subject='current',period='today',page=0)
            self.link=secrets.token_hex(16);self.seq=0;self.rxseq=0;self.actions={};self.last_rx=time.monotonic();self.last_rev=self.l.selection['selection_rev']
            if self.state.get('boot_id')!=b['boot_id']:
                if self.state.get('firmware_errors'):
                    self.state['previous_boot_errors']=self.state['firmware_errors'][-20:]
                self.state.update(boot_id=b['boot_id'],firmware_errors=[],heap_diagnostics=None,link_diagnostics=None)
            self.state.update(connected=True,firmware=b['firmware'],last_seen_ms=now(),error=None)
            self.send('welcome',dict(bridge_epoch=secrets.token_hex(16),**self.l.selection,heartbeat_ms=2000,offline_after_ms=6000,max_frame_bytes=8192,demo=False));self.snapshots();return
        if not self.link or d['link_id']!=self.link:
            counters['link_rejected']+=1;return
        if d['seq']<=self.rxseq:
            counters['seq_rejected']+=1;return
        if typ=='pong':
            if set(b)!={'monotonic_ms'} or type(b['monotonic_ms']) is not int or not 0<=b['monotonic_ms']<=9007199254740991:return
        elif typ=='usage_request':
            if set(b)!={'request_id','expected_usage_rev','subject','period','page'} or not isinstance(b['request_id'],str) or not HEX.fullmatch(b['request_id']):return
            if b['subject'] not in (*AGENTS,'all','current') or b['period'] not in ('today','7d','30d') or type(b['page']) is not int or not 0<=b['page']<=33 or type(b['expected_usage_rev']) is not int:return
            rid=b['request_id'];ack=self.usage_actions.get(rid)
            if ack is None:
                ok=b['expected_usage_rev']==self.usage_view['usage_rev']
                if ok:self.usage_view=dict(usage_rev=self.usage_view['usage_rev']+1,subject=b['subject'],period=b['period'],page=b['page'])
                ack=dict(request_id=rid,status='accepted' if ok else 'rejected',reason='ok' if ok else 'conflict',**self.usage_view);self.usage_actions[rid]=ack
                if len(self.usage_actions)>128:del self.usage_actions[next(iter(self.usage_actions))]
            self.send('usage_ack',ack)
            if self.usage:self.send('usage',project(self.usage,self.usage_view,self.l.selection['selected_agent']))
        elif typ=='action':
            if set(b)!={'action_id','kind','mode','agent_id','expected_selection_rev'} or not isinstance(b['action_id'],str) or not HEX.fullmatch(b['action_id']):return
            if b['kind']!='select' or b['mode'] not in ('auto','pinned') or b['agent_id'] not in AGENTS or type(b['expected_selection_rev']) is not int:return
            aid=b['action_id'];ack=self.actions.get(aid)
            if ack is None:
                ok=b['expected_selection_rev']==self.l.selection['selection_rev']
                if ok:self.l.select(b['mode'],b['agent_id']);self.l.auto()
                ack=dict(action_id=aid,status='accepted' if ok else 'rejected',**self.l.selection,reason='ok' if ok else 'conflict');self.actions[aid]=ack
                if len(self.actions)>128:del self.actions[next(iter(self.actions))]
            self.send('ack',ack)
            self.last_rev=self.l.selection['selection_rev'];self.snapshots()
        else:return
        if typ=='pong':counters['accepted_pong']+=1
        self.rxseq=d['seq'];self.last_rx=time.monotonic();self.state['last_seen_ms']=now()
    async def run(self):
        while not self.stopping:
            try:
                if self.l.paused:self.close();await asyncio.sleep(.1);continue
                if self.serial is None:
                    ports=[self.requested_port] if self.requested_port else sorted(p.device for p in list_ports.comports() if p.vid==0x303a)
                    if not ports:self.state['error']='device_not_found';await asyncio.sleep(1);continue
                    if len(ports)>1:self.state['error']='multiple_devices_select_port';await asyncio.sleep(1);continue
                    self.serial=serial.Serial(port=None,baudrate=115200,timeout=0,write_timeout=0,exclusive=True);self.serial.dtr=False;self.serial.rts=False;self.serial.port=ports[0];self.serial.open();self.state['port']=ports[0];self.last_rx=time.monotonic()
                chunk=self.serial.read(min(self.serial.in_waiting,16384))
                if chunk:
                    self.buffer.extend(chunk)
                    while b'\n' in self.buffer:
                        line,_,rest=self.buffer.partition(b'\n');self.buffer=bytearray(rest)
                        if len(line)<=8198:self.receive(line.rstrip(b'\r'))
                    if len(self.buffer)>8198:self.buffer.clear();raise ValueError('rx_overflow')
                t=time.monotonic()
                if self.link:
                    if t-self.last_rx>6:raise TimeoutError('heartbeat_timeout')
                    if self.last_rev!=self.l.selection['selection_rev']:
                        self.send('selection',self.l.selection);self.last_rev=self.l.selection['selection_rev'];self.snapshots()
                    elif t-self.last_projection>1:self.snapshots()
                    if t-self.last_ping>2:self.send('ping',{'monotonic_ms':int(t*1000)});self.last_ping=t
                elif t-self.last_rx>8:raise TimeoutError('handshake_timeout')
                self.flush_tx()
            except (OSError,ValueError,serial.SerialException,TimeoutError) as exc:
                self.state['reconnect_evidence']=dict(at_ms=now(),last_protocol_age_ms=int((time.monotonic()-self.last_rx)*1000),pending_tx_bytes=len(self.tx_buffer),protocol_rx=dict(self.state.get('protocol_rx',{})),heap_as_of_ms=(self.state.get('heap_diagnostics') or {}).get('as_of_ms'),invalid_protocol_json=self.state.get('invalid_protocol_json',0))
                self.state['reconnect_reason']=type(exc).__name__+':'+str(exc)[:120]
                self.state['reconnect_count']=self.state.get('reconnect_count',0)+1
                if self.state['error']!='protocol_mismatch':self.state['error']='serial_reconnect'
                self.close();await asyncio.sleep(1)
            await asyncio.sleep(.02)
        self.close()
