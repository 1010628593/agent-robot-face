'use strict';
const fs=require('node:fs');const os=require('node:os');const path=require('node:path');const crypto=require('node:crypto');
const {boundedText,cursorHeaders}=require('./cursor-usage');
const number=v=>typeof v==='number'&&Number.isFinite(v)&&v>=0?v:null;
const hash=s=>crypto.createHash('sha256').update(s).digest('hex');
function tokenIdentity(token){try{const claims=JSON.parse(Buffer.from(token.split('.')[1],'base64url').toString());return typeof claims.sub==='string'?claims.sub:null;}catch{return null;}}
function missing(provider,reason){return {id:provider,window_key:'unknown',provider,account_hash:null,agents:[provider],label:provider,used_pct:null,remaining:null,limit:null,used:null,unit:'percent',reset_ms:null,as_of_ms:Date.now(),availability:'unavailable',reason};}
async function request(url,headers){const r=await fetch(url,{headers,redirect:'error',signal:AbortSignal.timeout(12000)});if(!r.ok)throw new Error(r.status===401||r.status===403?'login_expired':'provider_unavailable');return JSON.parse(await boundedText(r,1048576));}
async function codex(){
 let auth;try{auth=JSON.parse(fs.readFileSync(path.join(process.env.CODEX_HOME||path.join(os.homedir(),'.codex'),'auth.json'),'utf8'));}catch{return [missing('codex','no_existing_login')];}
 const token=auth.tokens?.access_token;const account=auth.tokens?.account_id||tokenIdentity(token||'');if(!token)return [missing('codex','no_oauth_login')];if(!account)return [missing('codex','account_identity_unavailable')];
 const headers={Authorization:`Bearer ${token}`};if(account)headers['ChatGPT-Account-Id']=account;
 const data=await request('https://chatgpt.com/backend-api/wham/usage',headers);let windows=[];
 const extra=Array.isArray(data.additional_rate_limits)?data.additional_rate_limits.map((v,i)=>[v.limit_name||v.metered_feature||String(i),v.rate_limit||v]):Object.entries(data.additional_rate_limits||{}).map(([k,v])=>[v.limit_name||k,v.rate_limit||v]);
 for(const [bucket,value] of [['codex',data.rate_limit],...extra]){
  for(const key of ['primary_window','secondary_window']){const w=value?.[key];if(!w)continue;const pct=number(w.used_percent);windows.push({id:`codex:${hash(account||'live')}:${bucket}:${key}`,window_key:`${bucket}:${key}`,provider:'codex',account_hash:hash(account||'live'),agents:['codex'],label:(bucket==='codex'?'':(/spark/i.test(bucket)?'Spark ':String(bucket).replace(/[^a-zA-Z0-9_.-]/g,'').slice(0,12)+' '))+(key==='primary_window'?'Session':'Weekly'),used_pct:pct===null?null:Math.min(100,pct),remaining:pct===null?null:Math.max(0,100-pct),limit:100,used:pct,unit:'percent',reset_ms:number(w.reset_at)===null?null:w.reset_at*1000,as_of_ms:Date.now(),availability:pct===null?'unavailable':'available',reason:pct===null?'missing_usage':'',provenance:'codex.official.wham'});}
 }
 return windows.length?windows:[missing('codex','no_quota_windows')];
}
async function cursor(){
 const {headers,identity}=cursorHeaders();
 const data=await request('https://cursor.com/api/usage-summary',headers);const result=[];
 for(const [key,p] of Object.entries({plan:data.individualUsage?.plan,on_demand:data.individualUsage?.onDemand,team_pool:data.teamUsage?.pooled})){
  if(!p)continue;const used=number(p.used),limit=number(p.limit),pct=number(p.totalPercentUsed)??(used!==null&&limit>0?used/limit*100:null);
  result.push({id:`cursor:${hash('cursor:'+identity)}:${key}`,window_key:key,provider:'cursor',account_hash:hash('cursor:'+identity),agents:['cursor'],label:key,used_pct:pct===null?null:Math.min(100,pct),used:used===null?null:used/100,limit:limit===null?null:limit/100,remaining:number(p.remaining)===null?null:p.remaining/100,unit:'USD',reset_ms:Number.isFinite(Date.parse(data.billingCycleEnd))?Date.parse(data.billingCycleEnd):null,as_of_ms:Date.now(),availability:pct===null&&used===null?'unavailable':'available',reason:'',provenance:'cursor.official.usage-summary'});
 }return result.length?result:[missing('cursor','no_quota_windows')];
}
let cached=null,lastProbe=0;
async function collectQuotas(force=false){if(cached&&Date.now()-lastProbe<(force?30000:300000))return cached;lastProbe=Date.now();const out=[];for(const [provider,fn] of [['codex',codex],['cursor',cursor]]){try{out.push(...await fn());}catch(e){out.push(missing(provider,e.cause?.code==='UND_ERR_CONNECT_TIMEOUT'||e.name==='TimeoutError'?'network_timeout':['login_expired','provider_unavailable','response_limit','no_existing_login','account_identity_unavailable'].includes(e.message)?e.message:'quota_probe_failed'));}}out.push(missing('hermes','provider_adapter_not_configured'),missing('workbuddy','unsupported'));cached=out;return out;}
module.exports={collectQuotas};
