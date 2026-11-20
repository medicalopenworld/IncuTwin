// Genera ejemplos EXACTOS del payload de incubators/<SN>/state pasando la historia del simulador
// (tools/incunest_sim.py, escenario "demo") por la máquina de estados real (itw_state_machine.js).
// Son los ficheros de ejemplo para los tests del consumidor MQTT de la app.
//   node tb/gen_sim_fixtures.js            -> tb/fixtures/incubator-state-examples.json
const fs = require('fs');
const path = require('path');
const fn = new Function('msg', 'metadata', 'msgType',
  fs.readFileSync(path.join(__dirname, 'itw_state_machine.js'), 'utf8'));

let state = null; let cs = {}; const out = [];
const S = 1000, M = 60 * S;
let t = Date.parse('2026-11-20T10:00:00Z');
const base = { SN: 'SIM01', Control_active: false, Phototherapy_active: false, Control_mode: 'AIR',
  Temp_desired: 36.5, Air_temp: 28, HR1: 0, HR1_SQI: 0, SpO2_SQI: 0 };
let tel = Object.assign({}, base);

function step(msg, msgType, label) {
  const md = Object.assign({ deviceName: 'IncuNest-SIM01', SN: 'SIM01', ts: String(t), ss_country: 'TG' }, cs);
  if (state) md.ss_itw_state = state;
  const res = fn(msg, md, msgType);
  state = res.find(o => o.metadata.itw_out === 'ITW_STATE').msg.itw_state;
  const b = res.find(o => o.metadata.itw_out === 'ITW_BROKER');
  if (b && label) out.push({ case: label, topic: 'incubators/SIM01/state', retained: true, qos: 1, payload: b.msg });
}
function tick(n, label) { // n mensajes de telemetría cada 5 s; la temperatura sube hacia el objetivo
  for (let i = 0; i < n; i++) {
    t += 5 * S;
    const target = tel.Control_active ? tel.Temp_desired : 26;
    tel.Air_temp = Math.round((tel.Air_temp + Math.max(-0.6, Math.min(0.6, target - tel.Air_temp))) * 10) / 10;
    step(Object.assign({}, tel), 'POST_TELEMETRY_REQUEST', label);
  }
}

tick(2, 'libre (sin evento: no se publica nada)');
t += S; cs = { cs_baby_seq: '1', cs_baby_admission_epoch: String(Math.floor(t / 1000)) };
step({ baby_seq: 1, baby_admission_epoch: Math.floor(t / 1000) }, 'POST_ATTRIBUTES_REQUEST');
tel.Control_active = true; tick(1, 'ingreso con calor (baby_in)');
tick(12 * 6, 'heartbeat / calor calentando y luego estable');
tel.Phototherapy_active = true; tick(7, 'fototerapia (treatment_changed)');
tel.HR1 = 140; tel.HR1_SQI = 0.9; tel.SpO2_SQI = 0.9; tick(14, 'pulsioxímetro con bpm');
tel.HR1 = 0; tel.HR1_SQI = 0; tel.SpO2_SQI = 0; tel.Control_active = false; tel.Phototherapy_active = false;
t += 5 * S; step(Object.assign({}, tel, { baby_seq: 1, baby_kangaroo_event: true, baby_kangaroo_count: 1 }), 'POST_TELEMETRY_REQUEST', 'canguro (baby_parents)');
tick(12, 'con sus papás');
tel.Control_active = true; tick(7, 'vuelta a la incubadora (baby_back)');
t += 10 * M; step({}, 'INACTIVITY_EVENT', 'incubadora apagada (incubator_offline)');
tick(1, 'vuelve a mandar (incubator_online)');
tel.air_temp_high_alarm = true; tick(1, 'alarma térmica (thermo: alarm)');
tel.air_temp_high_alarm = false; tick(1, 'fin de alarma');
tel.Control_active = false; tel.Phototherapy_active = false;
t += 5 * S; step(Object.assign({}, tel, { baby_seq: 1, baby_outcome: 1, baby_discharge_epoch: Math.floor(t / 1000) }), 'POST_TELEMETRY_REQUEST', 'alta a casa (baby_out, baby: home)');
tick(2, 'después del alta');

// un ejemplo por tipo de evento + los heartbeat con estados distintos (sin repetir)
const seen = new Set(); const pick = [];
for (const o of out) {
  const p = o.payload;
  const key = [p.last_event, p.state, p.thermo, p.baby, p.treatments.join('+'), p.bpm === null ? 'nobpm' : 'bpm'].join('|');
  if (!seen.has(key)) { seen.add(key); pick.push(o); }
}
const dir = path.join(__dirname, 'fixtures');
fs.mkdirSync(dir, { recursive: true });
const file = path.join(dir, 'incubator-state-examples.json');
fs.writeFileSync(file, JSON.stringify({
  _about: 'Payloads reales de incubators/<SN>/state generados con tb/itw_state_machine.js (node tb/gen_sim_fixtures.js). ' +
    'Campos nuevos pueden aparecer: el consumidor debe ignorar los desconocidos. name/weight_g/age_d solo salen con ' +
    'itw_show_name y la app NO debe mostrarlos.',
  examples: pick }, null, 2) + '\n');
console.log(pick.length, 'ejemplos de', out.length, 'publicaciones ->', path.relative(process.cwd(), file));
for (const o of pick) console.log(' ', o.payload.event_seq, o.payload.last_event.padEnd(18), o.payload.state.padEnd(7), o.payload.thermo.padEnd(8), o.payload.baby.padEnd(8), JSON.stringify(o.payload.treatments), o.payload.bpm);
