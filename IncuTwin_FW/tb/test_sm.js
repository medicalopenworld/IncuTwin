const fs=require('fs');
const fn=new Function('msg','metadata','msgType', fs.readFileSync(require('path').join(__dirname,'itw_state_machine.js'),'utf8'));
let state=null; let evs=[];
function step(msg,msgType,ts){
  const md={deviceName:'IncuNest-TEST',SN:'TEST-001',ts:String(ts)}; if(state) md.ss_itw_state=state;
  const out=fn(msg,md,msgType);
  state=out.find(o=>o.metadata.itw_out==='ITW_STATE').msg.itw_state;
  const panel=out.find(o=>o.metadata.itw_out==='ITW_PANEL').msg;
  out.filter(o=>o.metadata.itw_out==='ITW_EVENT').forEach(o=>{evs.push(o.msg); console.log(new Date(ts).toISOString().slice(11,19), o.msg.event, JSON.stringify(o.msg.payload), 'stay='+o.msg.stay_id, 'panel.baby='+panel.baby,'thermo='+panel.thermo);});
}
let t=Date.parse('2026-09-14T08:00:00Z'); const S=1000, M=60*S, H=60*M;
// standby, perfil viejo en slot (baby_seq=5 pero sin terapia)
step({baby_seq:5,baby_admission_epoch:0},'POST_ATTRIBUTES_REQUEST',t);
step({Control_active:false,Phototherapy_active:false,Air_temp:25},'POST_TELEMETRY_REQUEST',t+5*S);
// enfermera crea bebé 6 y enciende calor en modo AIR a 36
t+=1*M; step({baby_seq:6,baby_admission_epoch:Math.floor(t/1000),baby_kangaroo_count:0},'POST_ATTRIBUTES_REQUEST',t);
step({Control_active:true,Control_mode:'AIR',Temp_desired:36,Air_temp:25,Phototherapy_active:false},'POST_TELEMETRY_REQUEST',t+2*S);
for(let i=1;i<=80;i++){ step({Control_active:true,Temp_desired:36,Air_temp:Math.min(36,25+i*0.2),Phototherapy_active:false,HR1:141,HR1_SQI:0.8},'POST_TELEMETRY_REQUEST',t+2*S+i*5*S); }
// estable 6 min más
for(let i=1;i<=80;i++){ step({Control_active:true,Temp_desired:36,Air_temp:36.1,Phototherapy_active:false,HR1:138,HR1_SQI:0.8},'POST_TELEMETRY_REQUEST',t+7*M+i*5*S); }
// fototerapia on
t+=15*M; step({Control_active:true,Temp_desired:36,Air_temp:36,Phototherapy_active:true},'POST_TELEMETRY_REQUEST',t);
// corte de red 10 min
step({},'INACTIVITY_EVENT',t+3*M); step({},'ACTIVITY_EVENT',t+13*M);
// canguro: apagan todo y responden canguro
t+=20*M; step({Control_active:false,Phototherapy_active:false,Air_temp:35},'POST_TELEMETRY_REQUEST',t);
step({baby_seq:6,baby_kangaroo_event:1,baby_kangaroo_count:1},'POST_TELEMETRY_REQUEST',t+5*S);
// vuelve a la hora
t+=1*H; step({Control_active:true,Temp_desired:36,Air_temp:34,Phototherapy_active:false},'POST_TELEMETRY_REQUEST',t);
// 3 días después alta a casa
t+=3*24*H; step({Control_active:false,Phototherapy_active:false},'POST_TELEMETRY_REQUEST',t);
step({baby_seq:6,baby_outcome:1,baby_discharge_epoch:Math.floor(t/1000),baby_admission_epoch:1,baby_thermo_min:4000,baby_phototherapy_min:120,baby_kangaroo_count:1},'POST_TELEMETRY_REQUEST',t+5*S);
step({baby_seq:0},'POST_ATTRIBUTES_REQUEST',t+6*S);
// nuevo bebé 7 sin alta explícita luego 7 h idle
t+=2*H; step({baby_seq:7,baby_admission_epoch:Math.floor(t/1000)},'POST_ATTRIBUTES_REQUEST',t);
step({Control_active:true,Control_mode:'SKIN',Temp_desired:36.5,Skin_temp:35},'POST_TELEMETRY_REQUEST',t+2*S);
t+=1*H; step({Control_active:false},'POST_TELEMETRY_REQUEST',t);
step({Control_active:false},'POST_TELEMETRY_REQUEST',t+5*H);
step({Control_active:false},'POST_TELEMETRY_REQUEST',t+6*H+1*M);
console.log('\nEVENT COUNT by type:', evs.reduce((a,e)=>(a[e.event]=(a[e.event]||0)+1,a),{}));
console.log('state bytes:', state.length);
