"""Fail-open native observer. Stdin discarded after one-way metadata extraction."""
import argparse,json,os,sys,fcntl
from pathlib import Path
from .adapters import native_event
from .runtime import now,ident

def main():
    p=argparse.ArgumentParser();p.add_argument('--agent',required=True,choices=['codex','cursor','hermes','workbuddy']);p.add_argument('--event');p.add_argument('--root',type=Path,required=True);a=p.parse_args()
    try:
        raw=sys.stdin.buffer.read(1048577)
        if len(raw)>1048576:return
        d=json.loads(raw);d['hook_event_name']=a.event or d.get('hook_event_name');e=native_event(a.agent,d)
        a.root.mkdir(parents=True,exist_ok=True);os.chmod(a.root,0o700)
        # Delivery receipt proves invocation only; never claims unverified WorkBuddy terminal semantics.
        receipt=a.root/('hook-receipt-'+a.agent+'.json');tmp=receipt.with_suffix('.tmp.'+str(os.getpid()));tmp.write_text(json.dumps({'at_ms':now(),'event':a.event if a.event in ['SessionStart','UserPromptSubmit','PreToolUse','PostToolUse','Stop','beforeSubmitPrompt','preToolUse','postToolUse','postToolUseFailure','stop','on_session_start','pre_llm_call','pre_tool_call','post_tool_call','on_session_end'] else 'other'}));tmp.chmod(0o600);tmp.replace(receipt)
        if a.agent=='workbuddy':
            pd=d.get('extra') if isinstance(d.get('extra'),dict) else {}
            candidate={'at_ms':now(),'event':a.event,'fields':sorted(k for k in d if isinstance(k,str) and len(k)<64 and k.replace('_','').isalnum())[:64]}
            for key in ('session_id','turn_id','tool_use_id','tool_call_id'):
                if d.get(key):candidate[key]=ident(d[key])
            for key in ('status','end_reason','stop_reason','reason','error_code'):
                value=d.get(key,pd.get(key))
                if value in ('completed','aborted','cancelled','canceled','interrupted','failed','error','stop','success'):candidate[key]=value
            for key in ('completed','failed','interrupted','stop_hook_active'):
                value=d.get(key,pd.get(key))
                if type(value) is bool:candidate[key]=value
            path=a.root/'workbuddy-candidates.jsonl'
            fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
            with os.fdopen(fd,'a') as f:
                fcntl.flock(f,fcntl.LOCK_EX)
                if f.tell()>262144:f.seek(0);f.truncate()
                f.write(json.dumps(candidate)+'\n')
        if e:
            fd=os.open(a.root/'hooks.jsonl',os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
            with os.fdopen(fd,'a') as f:
                fcntl.flock(f,fcntl.LOCK_EX);f.write(json.dumps(e,separators=(',',':'))+'\n');f.flush()
    except Exception:pass
if __name__=='__main__':main()
