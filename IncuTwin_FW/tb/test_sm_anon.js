// Bebé sin registrar: termorregulación y/o fototerapia => hay bebé (spec: cuidado implica bebé).
// Sin datos registrados (baby_seq=0) el panel muestra "bebé dentro" pero sin nombre/peso/días.
// Si luego se registra, la estancia anónima adopta el baby_seq (sin cerrar y reabrir).
const fs = require('fs');
const assert = require('assert');
const fn = new Function('msg', 'metadata', 'msgType',
  fs.readFileSync(require('path').join(__dirname, 'itw_state_machine.js'), 'utf8'));

let state = null; let cs = {};
function step(msg, msgType, ts) {
  // nombre/peso de un bebé anterior siguen en los client attributes: no deben salir sin registro
  const md = Object.assign({ deviceName: 'IncuNest-327_1', SN: '327', ts: String(ts),
    ss_itw_show_name: 'true', cs_baby_name: 'Antiguo', cs_baby_weight_g: '999' }, cs);
  if (state) md.ss_itw_state = state;
  const out = fn(msg, md, msgType);
  state = out.find(o => o.metadata.itw_out === 'ITW_STATE').msg.itw_state;
  return {
    st: JSON.parse(state),
    panel: out.find(o => o.metadata.itw_out === 'ITW_PANEL').msg,
    broker: (out.find(o => o.metadata.itw_out === 'ITW_BROKER') || {}).msg,
    events: out.filter(o => o.metadata.itw_out === 'ITW_EVENT').map(o => o.msg),
  };
}
const S = 1000, M = 60 * S, H = 60 * M;
let t = Date.parse('2026-11-18T10:00:00Z');

// 1) solo fototerapia, sin baby_seq: abre estancia anónima y publica en el broker
let r = step({ Control_active: false, Phototherapy_active: true, Air_temp: 26 }, 'POST_TELEMETRY_REQUEST', t);
assert.deepStrictEqual(r.events.map(e => e.event).filter(e => e !== 'heartbeat'), ['baby_in'], 'cuidado sin registro abre estancia');
assert.strictEqual(r.st.baby_state, 'in');
assert.strictEqual(r.st.stay_seq, 0);
assert.ok(/^327:0-\d+$/.test(r.st.stay_id), 'stay_id anónimo único: ' + r.st.stay_id);
assert.strictEqual(r.events[0].payload.admitted_at, null);
assert.ok(r.broker, 'publica estado en el broker');
assert.strictEqual(r.broker.state, 'baby');
assert.strictEqual(r.broker.baby, 'in');
assert.deepStrictEqual(r.broker.treatments, ['phototherapy']);
assert.strictEqual(r.broker.name, undefined, 'sin registro no sale el nombre');
assert.strictEqual(r.broker.weight_g, undefined);
assert.strictEqual(r.broker.age_d, undefined);
assert.strictEqual(r.panel.baby, 'in');
assert.strictEqual(r.panel.name, '');
const anonId = r.st.stay_id;

// 2) solo termorregulación: sigue dentro, sin eventos de estancia nuevos
r = step({ Control_active: true, Control_mode: 'AIR', Temp_desired: 36, Air_temp: 30, Phototherapy_active: false },
  'POST_TELEMETRY_REQUEST', t + 1 * M);
assert.ok(!r.events.some(e => e.event === 'baby_in' || e.event === 'baby_out'));
assert.strictEqual(r.st.baby_state, 'in');

// 3) la enfermera registra al bebé: se adopta el seq, misma estancia, ya sale el nombre
cs = { cs_baby_seq: '4', cs_baby_admission_epoch: String(Math.floor((t + 2 * M) / 1000)), cs_baby_name: 'Nuevo', cs_baby_weight_g: '1800' };
step({ baby_seq: 4, baby_admission_epoch: Math.floor((t + 2 * M) / 1000) }, 'POST_ATTRIBUTES_REQUEST', t + 2 * M);
r = step({ Control_active: true, Temp_desired: 36, Air_temp: 31, Phototherapy_active: false }, 'POST_TELEMETRY_REQUEST', t + 2 * M + 5 * S);
assert.ok(!r.events.some(e => e.event === 'baby_in' || e.event === 'baby_out'), 'adoptar no cierra ni reabre');
assert.strictEqual(r.st.stay_seq, 4);
assert.strictEqual(r.st.stay_id, anonId, 'conserva el stay_id para no partir el historial');
assert.strictEqual(r.panel.name, 'Nuevo');

// 4) sin cuidado durante t_idle (6 h): cierra la estancia
step({ Control_active: false, Phototherapy_active: false }, 'POST_TELEMETRY_REQUEST', t + 1 * H);
r = step({ Control_active: false, Phototherapy_active: false }, 'POST_TELEMETRY_REQUEST', t + 7 * H + 1 * M);
assert.deepStrictEqual(r.events.filter(e => e.event === 'baby_out').map(e => e.payload.reason), ['session_ended']);
assert.strictEqual(r.st.baby_state, 'out');

// 5) estado previo ya guardado sin bebé (como 327_1 al activar): el siguiente cuidado abre estancia
state = null; cs = {};
step({ Control_active: false, Phototherapy_active: false }, 'POST_TELEMETRY_REQUEST', t);
r = step({ Control_active: false, Phototherapy_active: true }, 'POST_TELEMETRY_REQUEST', t + 5 * S);
assert.deepStrictEqual(r.events.map(e => e.event).filter(e => e !== 'heartbeat'), ['baby_in']);
assert.strictEqual(r.broker.baby, 'in');

console.log('test_sm_anon OK');
