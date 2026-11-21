// La IncuNest manda Control_active / Control_mode solo cuando cambian (Phototherapy_active,
// Air_temp, Temp_desired cada minuto). Si el mensaje no los trae, vale el último valor guardado
// (latestTsKeyNames del nodo "estado previo + SN"). Caso real: la 297, habilitada con el control
// ya encendido, salía con thermo "off".
const fs = require('fs');
const assert = require('assert');
const fn = new Function('msg', 'metadata', 'msgType',
  fs.readFileSync(require('path').join(__dirname, 'itw_state_machine.js'), 'utf8'));

let state = null;
function step(msg, md, ts) {
  const m = Object.assign({ deviceName: '8944501606231786020', SN: '297', ts: String(ts) }, md);
  if (state) m.ss_itw_state = state;
  const out = fn(msg, m, 'POST_TELEMETRY_REQUEST');
  state = out.find(o => o.metadata.itw_out === 'ITW_STATE').msg.itw_state;
  return { st: JSON.parse(state), broker: (out.find(o => o.metadata.itw_out === 'ITW_BROKER') || {}).msg };
}
const M = 60000;
let t = Date.parse('2026-11-21T10:00:00Z');
const latest = { Control_active: 'true', Control_mode: 'AIR' };

// estado nuevo, el mensaje no trae Control_active: vale el último guardado
let r = step({ Phototherapy_active: true, Temp_desired: 37.1, Air_temp: 35.0 }, latest, t);
assert.strictEqual(r.st.control_active, true, 'control activo por el último valor');
assert.strictEqual(r.st.heat, 'heating');
assert.ok(r.broker.treatments.includes('heat') || r.broker.thermo === 'heating', 'el broker ve el calor');
assert.strictEqual(r.broker.thermo, 'heating');

// cerca de la consigna 5 min seguidos: estable
r = step({ Phototherapy_active: true, Temp_desired: 37.1, Air_temp: 36.95 }, latest, t += M);
r = step({ Phototherapy_active: true, Temp_desired: 37.1, Air_temp: 36.95 }, latest, t += 6 * M);
assert.strictEqual(r.st.heat, 'stable');

// el mensaje manda sobre el último guardado (el apagado llega en el propio mensaje)
r = step({ Control_active: false, Phototherapy_active: true }, latest, t += M);
assert.strictEqual(r.st.control_active, false, 'el mensaje manda');
assert.strictEqual(r.st.heat, 'off');

// sin Control_active ni en el mensaje ni guardado: se queda como estaba
state = null;
r = step({ Phototherapy_active: true }, {}, t += M);
assert.strictEqual(r.st.control_active, false);
console.log('ok test_sm_latest');
