'use strict';
// Read-only official export. Credentials and raw CSV remain in memory only.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path');
function cursorHeaders(){
 const {DatabaseSync}=require('node:sqlite');let db,token;
 const p=path.join(os.homedir(),'Library/Application Support/Cursor/User/globalStorage/state.vscdb');
 if(!fs.existsSync(p))throw new Error('no_existing_login');
 try{db=new DatabaseSync(p,{readOnly:true});token=db.prepare('SELECT value FROM ItemTable WHERE key = ?').get('cursorAuth/accessToken')?.value;}finally{db?.close();}
 if(!token)throw new Error('no_existing_login');
 let identity;try{identity=JSON.parse(Buffer.from(token.split('.')[1],'base64url').toString()).sub;}catch{}
 if(typeof identity!=='string'||!identity)throw new Error('account_identity_unavailable');
 return {headers:{Cookie:`WorkosCursorSessionToken=${encodeURIComponent(identity+'::'+token)}`},identity};
}
async function boundedText(response,limit){let size=0,parts=[];for await(const chunk of response.body){size+=chunk.length;if(size>limit)throw new Error('response_limit');parts.push(chunk);}return Buffer.concat(parts).toString('utf8');}
function csvRows(text){let rows=[],row=[],cell='',quoted=false;
 for(let i=0;i<text.length;i++){const c=text[i];if(c==='"'){if(quoted&&text[i+1]==='"'){cell+='"';i++;}else quoted=!quoted;}else if(!quoted&&(c===','||c==='\n')){row.push(cell.replace(/\r$/,''));cell='';if(c==='\n'){rows.push(row);row=[];if(rows.length>200000)throw new Error('row_limit');}}else cell+=c;}
 if(quoted)throw new Error('invalid_export');if(cell||row.length){row.push(cell.replace(/\r$/,''));rows.push(row);}return rows;
}
const n=v=>/^\d+$/.test(String(v))&&Number.isSafeInteger(Number(v))?Number(v):null;
function normalizeCSV(text,since,until){const rows=csvRows(text.replace(/^\uFEFF/,''));const header=rows.shift()||[];const ix=name=>header.indexOf(name);if(['Date','Model','Total Tokens'].some(k=>ix(k)<0))throw new Error('invalid_export');const buckets=new Map();
 for(const r of rows){if(r.length!==header.length)continue;const date=new Date(r[ix('Date')]);if(!Number.isFinite(date.getTime()))continue;const day=`${date.getFullYear()}-${String(date.getMonth()+1).padStart(2,'0')}-${String(date.getDate()).padStart(2,'0')}`;if(day<since||day>until)continue;
 const rawModel=r[ix('Model')],model=/^[a-zA-Z0-9_.:/@+ -]{1,96}$/.test(rawModel)&&!rawModel.includes('/Users/')&&!rawModel.includes('/home/')?rawModel:'unknown';
 const total=n(r[ix('Total Tokens')]),input=n(r[ix('Input (w/ Cache Write)')]),uncached=n(r[ix('Input (w/o Cache Write)')]),cache=n(r[ix('Cache Read')]),output=n(r[ix('Output Tokens')]);
 // Official export separates cache-write input, uncached input, cache-read and output.
 const inclusive=input!==null&&uncached!==null&&cache!==null?input+uncached+cache:null,write=input;
 const fields={total_tokens:total,input_tokens:inclusive,output_tokens:output,cache_read_tokens:cache,cache_write_tokens:write};
 if(Object.values(fields).some(v=>v!==null&&!Number.isSafeInteger(v)))throw new Error('metric_limit');
 if(total!==null&&inclusive!==null&&output!==null&&inclusive+output!==total){fields.input_tokens=null;fields.cache_write_tokens=null;}
 const key=JSON.stringify([day,model]);const existing=buckets.get(key);if(existing){for(const k of Object.keys(fields)){existing[k]=existing[k]===null||fields[k]===null?null:existing[k]+fields[k];if(existing[k]!==null&&!Number.isSafeInteger(existing[k]))throw new Error('metric_limit');}}
 else buckets.set(key,{date:day,agent:'cursor',model,...fields,actual_cost:null,coverage:{tokens:'partial',cost:'unknown'},provenance:'cursor.official.usage-events',cache_definition:'input includes cache read and write; native export may cover only current billing cycle'});
 if(buckets.size>10000)throw new Error('row_limit');}
 return [...buckets.values()];
}
let cache=null,lastAttempt=0;
async function collectCursorUsage(since,until,force=false){if(cache&&Date.now()-lastAttempt<(force?30000:300000))return cache;lastAttempt=Date.now();try{const {headers}=cursorHeaders();const response=await fetch('https://cursor.com/api/dashboard/export-usage-events-csv?strategy=tokens',{headers,redirect:'error',signal:AbortSignal.timeout(30000)});if(!response.ok)throw new Error(response.status===401||response.status===403?'login_expired':'provider_unavailable');const days=normalizeCSV(await boundedText(response,16*1024*1024),since,until);cache={days,as_of_ms:Date.now(),availability:'available',reason:null,snapshot_complete:false};}catch(e){cache={days:[],as_of_ms:null,availability:'unavailable',reason:['no_existing_login','account_identity_unavailable','login_expired','provider_unavailable','response_limit','invalid_export','row_limit','metric_limit'].includes(e.message)?e.message:'usage_probe_failed',snapshot_complete:false};}return cache;}
module.exports={cursorHeaders,boundedText,collectCursorUsage,normalizeCSV};
