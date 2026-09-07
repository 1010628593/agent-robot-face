'use strict';
// Native bill evidence only. No session text or identifiers leave this module.
const fs=require('node:fs');const path=require('node:path');const os=require('node:os');
function dayKey(seconds){const d=new Date(seconds*1000);return `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}`;}
function applyHermesActual(rows,since,until){
 const filename=path.join(process.env.HERMES_HOME||path.join(os.homedir(),'.hermes'),'state.db');if(!fs.existsSync(filename))return {availability:'unavailable',reason:'no_native_database'};
 let db;try{const {DatabaseSync}=require('node:sqlite');db=new DatabaseSync(filename,{readOnly:true});
 const columns=new Set(db.prepare('PRAGMA table_info(sessions)').all().map(r=>r.name));if(!['model','started_at','ended_at','actual_cost_usd','cost_status'].every(k=>columns.has(k)))return{availability:'unavailable',reason:'native_cost_schema_missing'};
 const floor=new Date(`${since}T00:00:00`).getTime()/1000,ceil=new Date(`${until}T23:59:59.999`).getTime()/1000;
 const native=db.prepare('SELECT model,started_at,ended_at,actual_cost_usd,cost_status FROM sessions WHERE started_at >= ? AND started_at <= ? AND actual_cost_usd IS NOT NULL LIMIT 10001').all(floor,ceil);
 if(native.length>10000)return{availability:'unavailable',reason:'native_cost_row_limit'};
 let applied=0;for(const s of native){if(typeof s.actual_cost_usd!=='number'||!Number.isFinite(s.actual_cost_usd)||s.actual_cost_usd<0||!['actual','exact','reported','billed'].includes(s.cost_status))continue;
 if(typeof s.started_at!=='number'||typeof s.ended_at!=='number'||s.ended_at<s.started_at||dayKey(s.started_at)!==dayKey(s.ended_at))continue;
 const row=rows.find(r=>r.agent==='hermes'&&r.date===dayKey(s.started_at)&&r.model===s.model);if(!row)continue;
 row.actual_cost={amount:(row.actual_cost?.amount||0)+s.actual_cost_usd,currency:'USD',provenance:'hermes_actual_cost_usd'};row.coverage.cost='partial';applied++;
 }return{availability:applied?'available':'unknown',reason:applied?'':'no_qualifying_native_actual_cost',native_rows_applied:applied};
 }catch{return{availability:'unavailable',reason:'native_cost_read_failed'};}finally{db?.close();}
}
module.exports={applyHermesActual};
