// IncuNest 353, 25-nov-2026: reloj de la IncuNest 1 h 42 min adelantado y air_sensor_alarm=true
// colgada desde el 17-nov. La IncuTwin enseñaba "¡Alarma!" y el control de temperatura con el
// control apagado, y no se enteraba de que había fototerapia.
//   node tb/test_sm_clock_alarm.js
const fs = require('fs');
const path = require('path');
const fn = new Function('msg', 'metadata', 'msgType',
  fs.readFileSync(path.join(__dirname, 'itw_state_machine.js'), 'utf8'));

let fails = 0;
function check(cond, what) { if (!cond) { fails++; console.log('FALLO:', what); } }

function run(steps) {
  let state = null, broker = [], panel = null, events = [];
  for (const [msg, type, ts] of steps) {
    const md = { deviceName: 'IncuNest-353_1', SN: '353', ts: String(ts), ss_country: 'TG' };
    if (state) md.ss_itw_state = state;
    const out = fn(msg, md, type);
    state = out.find(o => o.metadata.itw_out === 'ITW_STATE').msg.itw_state;
    panel = out.find(o => o.metadata.itw_out === 'ITW_PANEL').msg;
    out.filter(o => o.metadata.itw_out === 'ITW_EVENT').forEach(o => events.push(o.msg.event));
    const br = out.find(o => o.metadata.itw_out === 'ITW_BROKER');
    if (br) broker.push(br.msg);
  }
  return { st: JSON.parse(state), broker, panel, events };
}

const S = 1000, M = 60 * S;
const t0 = Date.now() - 30 * M; // en el pasado: la máquina no deja pasar horas futuras
const AHEAD = 102 * M;          // reloj de la IncuNest adelantado

// 1. Alarma térmica colgada, control apagado, fototerapia encendida: sin alarma ni calor.
{
  const r = run([
    [{ baby_seq: 4, baby_admission_epoch: Math.floor((t0 + AHEAD) / 1000) }, 'POST_ATTRIBUTES_REQUEST', t0],
    [{ air_sensor_alarm: true, Control_active: true, Control_mode: 'AIR', Temp_desired: 35.5, Air_temp: 25, Phototherapy_active: true }, 'POST_TELEMETRY_REQUEST', t0 + 5 * S],
    [{ Control_active: false, Phototherapy_active: true, Air_temp: 25 }, 'POST_TELEMETRY_REQUEST', t0 + 2 * M],
  ]);
  check(r.panel.thermo === 'off', 'control apagado con alarma colgada: thermo=' + r.panel.thermo);
  check(r.panel.photo === true, 'fototerapia en el panel');
  check(r.broker.every(b => b.thermo !== 'alarm'), 'el broker nunca recibe thermo=alarm');
  const last = r.broker[r.broker.length - 1];
  check(last && last.thermo === 'off' && last.treatments.indexOf('phototherapy') >= 0 && last.treatments.indexOf('heat') < 0,
    'último estado del broker: solo fototerapia, ' + JSON.stringify(last && last.treatments));
}

// 2. Con el control encendido, una alarma térmica cuenta como calor, no como alarma.
{
  const r = run([
    [{ Control_active: true, Control_mode: 'AIR', Temp_desired: 36, Air_temp: 30, air_temp_low_alarm: true }, 'POST_TELEMETRY_REQUEST', t0],
  ]);
  check(r.panel.thermo === 'heating', 'control encendido con alarma: thermo=' + r.panel.thermo);
}

// 3. Un mensaje con la hora de la IncuNest adelantada no frena los cambios ni el latido.
{
  const r = run([
    [{ Control_active: true, Control_mode: 'AIR', Temp_desired: 35.5, Air_temp: 25, Phototherapy_active: false }, 'POST_TELEMETRY_REQUEST', t0],
    [{ baby_seq: 4, baby_admission_epoch: Math.floor((t0 + AHEAD) / 1000) }, 'POST_ATTRIBUTES_REQUEST', t0 + AHEAD],
    [{ Control_active: true, Phototherapy_active: true, Air_temp: 25 }, 'POST_TELEMETRY_REQUEST', t0 + 1 * M],
    [{ Control_active: false, Phototherapy_active: true, Air_temp: 25 }, 'POST_TELEMETRY_REQUEST', t0 + 2 * M],
  ]);
  const srv = Date.now() + 5 * S;
  ['in_since', 'last_hb', 'last_tx_change', 'last_seen'].forEach(k =>
    check(!r.st[k] || r.st[k] <= srv, k + ' en el futuro: ' + new Date(r.st[k]).toISOString()));
  check(r.events.filter(e => e === 'treatment_changed').length >= 2, 'treatment_changed tras la hora adelantada: ' + r.events.join(','));
  const last = r.broker[r.broker.length - 1];
  check(last && last.treatments.join(',') === 'phototherapy', 'el broker acaba con solo fototerapia: ' + JSON.stringify(last && last.treatments));
}

// 4. Un estado ya guardado con fechas futuras (lo que hay hoy en la 353) se cura solo.
{
  const future = Date.now() + AHEAD;
  const st = { v: 2, seq: 10, online: true, last_seen: t0, control_active: false, photo: false, spo2: false,
    hr: null, hr_q: 'none', control_mode: 'AIR', temp_desired: 35.5, alarms: { air_sensor_alarm: true },
    stable_since: null, heat: 'alarm', baby_seq: 4, admission_epoch: Math.floor(future / 1000), kangaroo_count: 0,
    thermo_min: 0, photo_min: 0, baby_state: 'in', stay_seq: 4, stay_id: '353:4', in_since: future,
    idle_since: null, parents_since: null, out_since: future, out_seq: 2, last_hb: future,
    last_tx: JSON.stringify({ heat: 'alarm', photo: false, spo2: false }), last_tx_change: future, care: true };
  const md = { deviceName: 'IncuNest-353_1', SN: '353', ts: String(Date.now() - 5 * S), ss_country: 'TG', ss_itw_state: JSON.stringify(st) };
  const out = fn({ Control_active: false, Phototherapy_active: true, Air_temp: 25 }, md, 'POST_TELEMETRY_REQUEST');
  const br = out.find(o => o.metadata.itw_out === 'ITW_BROKER');
  check(br && br.msg.thermo === 'off' && br.msg.treatments.join(',') === 'phototherapy',
    'estado futuro guardado: el siguiente mensaje ya publica solo fototerapia, ' + JSON.stringify(br && br.msg));
}

if (fails) { console.log(fails + ' fallo(s)'); process.exit(1); }
console.log('ok test_sm_clock_alarm');
