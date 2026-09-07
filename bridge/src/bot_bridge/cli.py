"""User service lifecycle. Installer touches only explicit observer entries."""
from __future__ import annotations
import argparse,datetime,json,os,pathlib,plistlib,shlex,shutil,subprocess,sys,urllib.request
from .service import DEFAULT_ROOT
from .runtime import Ledger
PROJECT=pathlib.Path(__file__).resolve().parents[3]
LABEL='com.agentrobotface.bridge'

def api(action=None):
    req=urllib.request.Request('http://127.0.0.1:17940/v1/'+('control' if action else 'state'),data=json.dumps(action).encode() if action else None,headers={'Content-Type':'application/json','Authorization':'Bearer '+(DEFAULT_ROOT/'control.token').read_text().strip()})
    with urllib.request.build_opener(urllib.request.ProxyHandler({})).open(req,timeout=5) as r:return json.load(r)
def launch(*args,check=True):return subprocess.run(['/bin/launchctl',*args],check=check,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

def hook_entries(root):
    common=[str(PROJECT/'.venv-bridge/bin/python'),'-m','bot_bridge.hook','--root',str(root)]
    # shell export applies only to observer; no environment secret copied to installer.
    prefix='PYTHONPATH='+shlex.quote(str(PROJECT/'bridge/src'))+' '
    return {a:{e:prefix+shlex.join(common+['--agent',a,'--event',e]) for e in events} for a,events in {'codex':['SessionStart','UserPromptSubmit','PermissionRequest','Stop'],'cursor':['beforeSubmitPrompt','preToolUse','postToolUse','postToolUseFailure','stop'],'workbuddy':['SessionStart','UserPromptSubmit','PreToolUse','PostToolUse','Stop'],'hermes':['on_session_start','pre_llm_call','pre_tool_call','post_tool_call','on_session_end']}.items()}

def write_atomic(p,text):
    p.parent.mkdir(parents=True,exist_ok=True);tmp=p.with_suffix(p.suffix+'.bot-tmp');tmp.write_text(text);tmp.chmod(0o600);tmp.replace(p)

def install_hooks(root):
    root.mkdir(parents=True,exist_ok=True);entries=hook_entries(root);backup=root/'backups'/datetime.datetime.now().strftime('%Y%m%dT%H%M%S%f');backup.mkdir(parents=True,mode=0o700)
    record=[];changed=[]
    try:
        for a,ev in entries.items():
            if a=='hermes':continue
            p=pathlib.Path.home()/('.'+a)/('settings.json' if a=='workbuddy' else 'hooks.json')
            before=p.read_text() if p.exists() else None;d=json.loads(before or '{}');hooks=d.setdefault('hooks',{})
            if a=='cursor':d.setdefault('version',1)
            for e,cmd in ev.items():
                entry={'command':cmd,'timeout':2} if a=='cursor' else {'hooks':[{'type':'command','command':cmd,'timeout':2}]}
                created=e not in hooks;values=hooks.setdefault(e,[])
                if entry not in values:values.append(entry)
                record.append({'path':str(p),'event':e,'entry':entry,'event_created':created})
            if before is not None:write_atomic(backup/(a+'.json'),before)
            changed.append((p,before));write_atomic(p,json.dumps(d,indent=2)+'\n')
        # Hermes YAML: insert a named block into each hook list. Keep every existing byte/secret intact.
        p=pathlib.Path.home()/'.hermes/config.yaml';before=p.read_text() if p.exists() else '';text=before
        for e,cmd in entries['hermes'].items():
            # Hermes uses shlex, not shell. Wrapper is executable and carries only fixed module arguments.
            wrapper=root/'hooks'/('hermes-'+e+'.sh');write_atomic(wrapper,'#!/bin/sh\nexport PYTHONPATH='+shlex.quote(str(PROJECT/'bridge/src'))+'\nexec '+shlex.join([str(PROJECT/'.venv-bridge/bin/python'),'-m','bot_bridge.hook','--root',str(root),'--agent','hermes','--event',e])+'\n');wrapper.chmod(0o700)
            line='    - command: '+str(wrapper)+' # agent-robot-face\n      timeout: 2\n'
            if line in text:continue
            import re
            match=re.search(r'^  '+e+r':\s*$',text,re.M)
            if match:text=text[:match.end()]+'\n'+line.rstrip('\n')+text[match.end():]
            else:
                m=re.search(r'^hooks:\s*$',text,re.M)
                if m:text=text[:m.end()]+'\n  '+e+':\n'+line.rstrip('\n')+text[m.end():]
                else:text+='\nhooks:\n  '+e+':\n'+line
            record.append({'path':str(p),'yaml_line':line,'event':e,'event_created':match is None})
        write_atomic(backup/'hermes.yaml',before);changed.append((p,before));write_atomic(p,text)
        manifest=root/'installed-hooks.json';old=json.loads(manifest.read_text()) if manifest.exists() else []
        for r in record:
            if r not in old:old.append(r)
        write_atomic(manifest,json.dumps(old,indent=2)+'\n')
    except Exception:
        for p,before in reversed(changed):
            if before is None:p.unlink(missing_ok=True)
            else:write_atomic(p,before)
        raise
    l=Ledger(root);l.put('installation',{'sources':list(entries),'backup':str(backup)});l.db.close()
    return {'hooks_configured':list(entries),'backup':str(backup),'hermes_consent':'Native first-use consent still applies; installer does not auto-accept or change source security settings.'}

def uninstall_hooks(root):
    manifest=root/'installed-hooks.json'
    if not manifest.exists():return
    for r in json.loads(manifest.read_text()):
        p=pathlib.Path(r['path'])
        if not p.exists():continue
        if 'yaml_line' in r:
            text=p.read_text().replace(r['yaml_line'],'')
            if r.get('event_created'):
                import re
                text=re.sub(r'^  '+re.escape(r['event'])+r':[^\S\n]*\n(?=  [^ ]|[^ ]|\Z)','',text,flags=re.M)
            write_atomic(p,text);continue
        d=json.loads(p.read_text());values=d.get('hooks',{}).get(r['event'],[])
        if r['entry'] in values:values.remove(r['entry'])
        if r.get('event_created') and not values:d.get('hooks',{}).pop(r['event'],None)
        # Empty containers remain rather than removing user-owned keys.
        write_atomic(p,json.dumps(d,indent=2)+'\n')
    manifest.unlink();l=Ledger(root);l.put('installation',{});l.db.close()

def install(root,only_hooks=False):
    node=os.environ.get('BOT_NODE') or shutil.which('node')
    if not only_hooks and not node:raise RuntimeError('Node.js 22.15+ is required for usage collection')
    result=install_hooks(root)
    if only_hooks:return result
    plist=pathlib.Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist';logs=root/'logs';logs.mkdir(exist_ok=True)
    data=dict(Label=LABEL,EnvironmentVariables={'BOT_NODE':node},ProgramArguments=[str(PROJECT/'tools/bridge'),'serve'],RunAtLoad=True,KeepAlive=True,WorkingDirectory=str(PROJECT),StandardOutPath=str(logs/'service.log'),StandardErrorPath=str(logs/'error.log'),ProcessType='Background',ThrottleInterval=5)
    write_atomic(plist,plistlib.dumps(data).decode());launch('bootout',f'gui/{os.getuid()}',str(plist),check=False);launch('enable',f'gui/{os.getuid()}/{LABEL}');launch('bootstrap',f'gui/{os.getuid()}',str(plist));l=Ledger(root);l.put('launch_at_login',True);l.db.close();return result|{'launch_agent':str(plist)}

def doctor():
    result={'python':sys.version.split()[0],'project':str(PROJECT),'launch_agent_installed':(pathlib.Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist').exists(),'source_settings':{a:(pathlib.Path.home()/('.'+a)).exists() for a in ['codex','cursor','hermes','workbuddy']}}
    try:result['state']=api()
    except Exception as e:result['service_error']=type(e).__name__
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('command',choices=['serve','install','repair-hooks','uninstall','doctor','state','pause','resume','auto','pin','flash']);p.add_argument('rest',nargs=argparse.REMAINDER);a=p.parse_args()
    if a.command=='serve':
        from .service import main as serve
        sys.argv=[sys.argv[0]]+a.rest;serve();return
    if a.command in ('install','repair-hooks'):result=install(DEFAULT_ROOT,a.command=='repair-hooks')
    elif a.command=='uninstall':
        plist=pathlib.Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist';launch('bootout',f'gui/{os.getuid()}',str(plist),check=False);uninstall_hooks(DEFAULT_ROOT);plist.unlink(missing_ok=True);result={'uninstalled':True,'ledger_retained':True,'backups_retained':True}
    elif a.command=='doctor':result=doctor()
    elif a.command=='state':result=api()
    elif a.command in ('pause','resume'):result=api({'action':a.command})
    elif a.command in ('auto','pin'):
        state=api();d={'action':a.command,'expected_selection_rev':state['selection']['selection_rev']}
        if a.command=='pin':d['agent_id']=a.rest[0] if a.rest else ''
        result=api(d)
    else:
        command=a.rest[1:] if a.rest and a.rest[0]=='--' else a.rest
        if not command:p.error('flash requires -- command arguments')
        state=api();was_paused=state['bridge']['paused'];api({'action':'pause'})
        try:code=subprocess.run(command).returncode
        finally:
            if not was_paused:api({'action':'resume'})
        sys.exit(code)
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
