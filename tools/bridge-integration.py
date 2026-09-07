#!/usr/bin/env python3
"""Isolated metadata/protocol/lifecycle integration scenarios. Never real user sources."""
import asyncio,json,os,pathlib,pty,sys,tempfile,time,threading
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'bridge/src'))
from bot_bridge.runtime import Ledger,now,ident
from bot_bridge.usb import USB
from bot_bridge.adapters import Adapters,native_event
from bot_bridge import cli

def run():
    checks=[]
    with tempfile.TemporaryDirectory(prefix='bot-bridge-integration-') as temp:
        root=pathlib.Path(temp);l=Ledger(root/'store');t=now()+10
        l.emit('codex','s','old','start',t);l.emit('codex','s','new','start',t+20);l.emit('codex','s','old','error',t+50)
        assert l.latest('codex')['run']==ident('new');checks.append('late old terminal cannot replace newer turn')
        l.emit('codex','s','new','tool_start',t+60,'terminal','A');l.emit('codex','s','new','tool_start',t+61,'terminal','B');l.emit('codex','s','new','tool_end',t+62,'terminal','A')
        l.emit('codex','s','new','tool_end',t+60,tool_id='A')
        assert l.latest('codex')['state']=='tool'
        l.emit('codex','s','new','waiting',t+63,tool_id='B',reason='approval');l.emit('codex','s','new','tool_start',t+64,tool_id='C');l.emit('codex','s','new','tool_end',t+65,tool_id='C')
        assert l.latest('codex')['state']=='waiting' and l.focus()['reason']=='approval';checks.append('concurrent tools and approval wait stay independent')
        l.emit('hermes','order','order','start',t);l.emit('hermes','order','order','tool_start',t+1,tool_id='A');l.emit('hermes','order','order','tool_start',t+3,tool_id='B');l.emit('hermes','order','order','tool_end',t+2,tool_id='A');l.emit('hermes','order','order','tool_end',t+4,tool_id='B');assert l.latest('hermes')['state']=='working';l.emit('hermes','order','order','done',t+5);checks.append('out-of-order independent tool completion')
        l.emit('codex','s','new','cancelled',t+70);l.emit('codex','s','new','tool_end',t+71,tool_id='B');assert l.latest('codex')['state']=='cancelled'
        before=l.db.execute('select count(*) from events').fetchone()[0];l.emit('codex','s','new','cancelled',t+90);assert l.db.execute('select count(*) from events').fetchone()[0]==before;checks.append('terminal proof and dedup survive late tool completion')
        l.emit('cursor','cs','c1','start',t+100);l.emit('cursor','cs','c1','waiting',t+110,reason='input');l.auto();assert l.selection['selected_agent']=='cursor'
        l.emit('hermes','hs','h1','start',t+120);l.emit('hermes','hs','h1','waiting',t+130);l.auto();assert l.selection['selected_agent']=='cursor'
        l.select('pinned','workbuddy');l.auto();assert l.selection['selected_agent']=='workbuddy';checks.append('stable equal priority and pin cannot be stolen')
        l.emit('workbuddy','old','history','error',t-86400000);assert not l.pending('workbuddy',ident('history'));checks.append('historical replay does not create new attention')
        l.select('pinned','codex');l.record_usage('codex','usage','total_tokens',1000,'exact',t-86400000);l.record_usage('codex','usage','total_tokens',1100,'exact',t);assert next(m for m in l.stats()['metrics'] if m['key']=='total_tokens')['value'] is None;l.record_usage('codex','usage','total_tokens',1200,'exact',t+1);metric=next(m for m in l.stats()['metrics'] if m['key']=='total_tokens');assert metric['value']==100
        l.record_usage('codex','unknown','cost_usd_micros',5000,'estimated',t);assert next(m for m in l.stats()['metrics'] if m['key']=='cost_usd_micros')['value'] is None;checks.append('cumulative baseline and cross-day deltas; missing cost stays null')
        sel=l.selection.copy();l.db.close();l=Ledger(root/'store');assert l.selection==sel;checks.append('selection ledger restart persistence')
        a=Adapters(l,root/'fake-home')
        context={};a.workbuddy({'id':'u','type':'message','role':'user','timestamp':t},context,'wb');a.workbuddy({'id':'r','parentId':'u','type':'message','role':'assistant','status':'incomplete','timestamp':t+10},context,'wb');a.workbuddy({'id':'meta','parentId':'r','type':'message','role':'user','providerData':{'isMeta':True},'timestamp':t+20},context,'wb');a.workbuddy({'id':'result','parentId':'meta','type':'message','role':'assistant','status':'completed','timestamp':t+30},context,'wb');assert l.latest('workbuddy')['state']=='unknown';checks.append('WorkBuddy meta response cannot rewrite incomplete human run')
        assert native_event('cursor',{'hook_event_name':'stop','conversation_id':'x','generation_id':'y','status':'aborted'})['kind']=='cancelled'
        assert native_event('hermes',{'hook_event_name':'on_session_end','session_id':'x','extra':{'turn_id':'y','failed':True,'completed':False,'platform':'cli'}})['kind']=='error'
        assert native_event('workbuddy',{'hook_event_name':'Stop','session_id':'x','turn_id':'y'}) is None;checks.append('native event schemas retain verified boundaries')
        # Real POSIX PTY exercises serial bytes and framing, isolated from hardware.
        master,slave=pty.openpty();usb=USB(l,os.ttyname(slave));import serial
        usb.serial=serial.Serial(os.ttyname(slave),115200,timeout=0,write_timeout=.5,exclusive=True)
        collected=bytearray();os.set_blocking(master,False);draining=True
        def drain():
            while draining:
                try:collected.extend(os.read(master,32768))
                except BlockingIOError:pass
                time.sleep(.001)
        reader=threading.Thread(target=drain,daemon=True);reader.start()
        hello={'v':3,'type':'hello','link_id':None,'seq':0,'body':{'device_id':'agent-robot-face','boot_id':'1'*32,'firmware':'3.0.0','display':{'width':466,'height':466},'min_version':3,'max_version':3,'handshake_id':'2'*32}}
        usb.receive(b'@bot '+json.dumps(hello).encode())
        # Drive the same bounded TX pump used by USB.run; receive only enqueues.
        for _ in range(32):usb.flush_tx()
        assert not usb.tx_buffer
        time.sleep(.02);frames=bytes(collected);assert b'"type":"welcome"' in frames and b'"type":"stats"' in frames
        link=usb.link;rev=l.selection['selection_rev'];action={'v':3,'type':'action','link_id':link,'seq':1,'body':{'action_id':'3'*32,'kind':'select','mode':'pinned','agent_id':'cursor','expected_selection_rev':rev}}
        usb.receive(b'@bot '+json.dumps(action).encode());assert l.selection['selected_agent']=='cursor';r=l.selection['selection_rev'];action['seq']=2;usb.receive(b'@bot '+json.dumps(action).encode());assert l.selection['selection_rev']==r
        usb.receive(b'@bot {"v":2,"v":2}');usb.receive(b'@diag {"render_updates":2,"window_ms":5000,"avg_us":100,"max_us":120}');assert usb.state['diagnostics']['render_updates']==2
        usb.close();draining=False;reader.join();os.close(master);os.close(slave);checks.append('PTY v3 welcome full snapshots action dedup invalid frame and diag isolation')
        # Minimal-merge fixture installer and rollback ownership. No launchctl or real HOME writes.
        home=root/'install-home';home.mkdir();original_home=pathlib.Path.home
        pathlib.Path.home=classmethod(lambda cls:home)
        try:
            for agent,filename in [('cursor','hooks.json'),('codex','hooks.json'),('workbuddy','settings.json')]:
                p=home/('.'+agent)/filename;p.parent.mkdir();p.write_text(json.dumps({'keep':'untouched','hooks':{'Stop':[{'hooks':[{'command':'existing-keyboard','type':'command'}]}]}}))
            p=home/'.hermes/config.yaml';p.parent.mkdir();p.write_text('unchanged: value\nhooks:\n  pre_llm_call:\n    - command: existing-memmy\n      timeout: 10\nother: retained\n')
            cli.install_hooks(root/'install-store');first=p.read_text();cli.install_hooks(root/'install-store');assert p.read_text()==first
            cli.uninstall_hooks(root/'install-store');assert 'agent-robot-face' not in p.read_text() and 'existing-memmy' in p.read_text() and 'other: retained' in p.read_text()
            assert json.loads((home/'.cursor/hooks.json').read_text())['keep']=='untouched';checks.append('idempotent installer and owned-entry uninstall preserve existing hooks')
        finally:pathlib.Path.home=original_home
        l.db.close()
    print(json.dumps({'passed':len(checks),'checks':checks},indent=2))
if __name__=='__main__':run()

# Endpoint integration uses a subprocess and isolated store, never the installed service.
def api_scenarios():
    import subprocess,urllib.request,urllib.error
    project=pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='bot-api-integration-') as td:
        env=dict(os.environ,PYTHONPATH=str(project/'bridge/src'))
        proc=subprocess.Popen([sys.executable,'-m','bot_bridge.service','--root',td,'--port','17941','--serial-port','/tmp/bot-integration-no-device','--no-sources'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        def request(data=None,token=None,origin=None):
            headers={'Content-Type':'application/json'}
            if token:headers['Authorization']='Bearer '+token
            if origin:headers['Origin']=origin
            req=urllib.request.Request('http://127.0.0.1:17941/v1/'+('control' if data else 'state'),data=json.dumps(data).encode() if data else None,headers=headers)
            try:
                with urllib.request.build_opener(urllib.request.ProxyHandler({})).open(req,timeout=2) as r:return r.status,json.load(r)
            except urllib.error.HTTPError as e:return e.code,json.load(e)
        try:
            for _ in range(50):
                try:code,state=request();break
                except (OSError,urllib.error.URLError):time.sleep(.05)
            else:
                proc.terminate();proc.wait(timeout=5);raise AssertionError('service did not start: '+proc.stderr.read().decode())
            assert code==200 and state['bridge']['demo'] is False and all(s['active_sessions'] is None for s in state['sources'])
            token=(pathlib.Path(td)/'control.token').read_text().strip()
            assert request({'action':'pause'})[0]==401
            assert request({'action':'pause'},token,'https://attacker.invalid')[0]==403
            assert request({'action':'pin','agent_id':'cursor','expected_selection_rev':999},token)[0]==409
            assert request({'action':'pin','agent_id':'cursor','expected_selection_rev':state['selection']['selection_rev']},token)[0]==200
            assert request({'action':'pause'},token)[1]['paused'] is True
            assert request({'action':'resume'},token)[1]['paused'] is False
            print(json.dumps({'api_integration':'PASS','checks':['real loopback server','no-source null metrics','bearer authentication','browser origin forbidden','atomic revision conflict','pin','pause release','resume']}))
        finally:proc.terminate();proc.wait(timeout=5)
if __name__=='__main__':api_scenarios()
