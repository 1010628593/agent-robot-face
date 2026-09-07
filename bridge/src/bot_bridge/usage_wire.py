"""Bounded USB v3 projections; all business selection remains in the lifecycle ledger."""
import hashlib,json,re
from .runtime import AGENTS,now

def wire_label(s,limit=24):
    return ''.join(c for c in str(s or '') if 32<=ord(c)<127)[:limit] or 'unknown'
def quota_stale(q):return not q.get('as_of_ms') or now()-q['as_of_ms']>900000

def project(store,view,current):
    subject=current if view['subject']=='current' else view['subject'];data=store.query(subject,view['period']);all_data=store.query('all',view['period']);s=data['summary'];costs=s['actual_costs'];cost=costs[0] if len(costs)==1 and costs[0]['amount']<=9007199254 else None
    summary=dict(agent_id=subject,total=s['total_tokens'],input=s['input_tokens'],output=s['output_tokens'],cache_read=s['cache_read_tokens'],cache_write=s['cache_write_tokens'],cost_micros=round(cost['amount']*1000000) if cost else None,cost_currency=cost['currency'] if cost else None,cost_coverage=cost['coverage'] if cost else 'unknown',cost_source=(cost['provenance'][0] if len(cost['provenance'])==1 else 'multiple_native_sources') if cost else '',coverage=s['coverage'])
    def quotas_for(a):return sorted([q for q in all_data['quotas'] if a in q['agents']],key=lambda q:(0 if q['provider']=='codex' and q['window_key'].startswith('codex:') else 1,q.get('reset_ms') or 9007199254740991,q['window_key'],q['account_hash'] or ''))
    agents=[]
    for a in AGENTS:
        a_sum=next(r for r in all_data['agents'] if r['id']==a);q=next((q for q in quotas_for(a) if q['availability']=='available' and q.get('used_pct') is not None and not quota_stale(q)),None)
        agents.append(dict(id=a,total=a_sum['total_tokens'],used_pct=q.get('used_pct') if q else None,available=a_sum['total_tokens'] is not None or q is not None))
    offset=view['page']*3;qs=data['quotas'];models=data['models'];quotas=[]
    for q in qs[offset:offset+3]:
        identity='|'.join(str(q[k] or '') for k in ('provider','account_hash','window_key'))
        aid=subject if subject!='all' and subject in q['agents'] else next((a for a in AGENTS if a in q['agents']),'codex')
        quotas.append(dict(id=hashlib.sha256(identity.encode()).hexdigest()[:32],agent_id=aid,label=wire_label(q['label']),used_pct=q['used_pct'],reset_ms=q['reset_ms'],stale=quota_stale(q),availability=q['availability']))
    result=dict(**view,host_now_ms=now(),data_rev=data['revision'],as_of_ms=data['as_of_ms'],stale=data['stale'],status=data['status'],summary=summary,agents=agents,quotas=quotas,quota_total=min(32,len(qs)),models=[{'label':wire_label(m['model'],32),'total':m['total_tokens']} for m in models[offset:offset+3]],model_total=min(100,data['model_count']),history=[{'day':h['date'],'total':h['total_tokens']} for h in data['history']])
    if len(json.dumps(result,separators=(',',':'),allow_nan=False).encode())>7600:raise ValueError('usage_wire_limit')
    return result
