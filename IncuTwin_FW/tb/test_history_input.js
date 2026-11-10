// Adaptadores de entrada del historial (itw_history_input.js) y su paso por itw_history.js,
// como los encadena ThingsBoard. node tb/test_history_input.js
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const inputSrc = fs.readFileSync(path.join(__dirname, 'itw_history_input.js'), 'utf8');
const histFn = new Function('msg', 'metadata', 'msgType', fs.readFileSync(path.join(__dirname, 'itw_history.js'), 'utf8'));
const adapter = mode => new Function('msg', 'metadata', 'msgType', inputSrc.replace("'__MODE__'", JSON.stringify(mode)));

const T = Date.parse('2026-11-10T10:00:00Z');
const D = 86400000;
const incMd = (extra) => Object.assign({
  deviceName: 'IncuNest-353', ts: String(T), ss_country: 'cm', ss_itw_show_name: 'true', cs_baby_name: 'Lucía',
  ss_itw_panels: JSON.stringify(['ITW-DEEF3C', 'ITW-2640-0007']), ss_itw_panel_count: '2',
  ss_itw_state: JSON.stringify({ v: 2, baby_state: 'in', stay_id: '353:17', in_since: T - 5 * D }),
}, extra || {});

const tests = [];
const test = (n, f) => tests.push([n, f]);

test('evento baby_in: una entrada por panel', () => {
  const ev = { event: 'baby_in', stay_id: '353:18', ts: new Date(T).toISOString(), payload: {} };
  const out = adapter('event')(ev, incMd({ ss_itw_state: JSON.stringify({ v: 2, baby_state: 'none' }) }), 'ITW_EVENT');
  assert.ok(Array.isArray(out.msg) && out.msg.length === 2);
  assert.deepStrictEqual(out.msg.map(m => m.panel), ['ITW-DEEF3C', 'ITW-2640-0007']);
  const m = out.msg[0];
  assert.strictEqual(m.kind, 'stay_open');
  assert.strictEqual(m.in_since, T);
  assert.strictEqual(m.country, 'CM');
  assert.strictEqual(m.panel_count, 2);
});

test('evento baby_out con fallecimiento: outcome null', () => {
  const ev = { event: 'baby_out', stay_id: '353:17', ts: new Date(T).toISOString(),
               payload: { reason: 'discharged', duration_days: 5.4, outcome: null } };
  const out = adapter('event')(ev, incMd(), 'ITW_EVENT');
  assert.strictEqual(out.msg[0].kind, 'stay_close');
  assert.strictEqual(out.msg[0].outcome, null);
  assert.strictEqual(out.msg[0].duration_days, 5.4);
});

test('sin consentimiento: no viaja el nombre', () => {
  const ev = { event: 'baby_in', stay_id: '353:18', ts: new Date(T).toISOString(), payload: {} };
  const out = adapter('event')(ev, incMd({ ss_itw_show_name: 'false' }), 'ITW_EVENT');
  assert.strictEqual(out.msg[0].name, '');
});

test('IncuNest sin paneles: array vacio', () => {
  const ev = { event: 'baby_in', stay_id: '353:18', ts: new Date(T).toISOString(), payload: {} };
  const out = adapter('event')(ev, incMd({ ss_itw_panels: undefined, ss_itw_panel_count: undefined }), 'ITW_EVENT');
  assert.deepStrictEqual(out.msg, []);
});

test('hold aceptado -> historial del panel', () => {
  const log = { itw_hand_hold: JSON.stringify({ source: 'panel', minutes: 5, panel: 'ITW-DEEF3C', baby_state: 'in' }) };
  const a = adapter('hold')(log, incMd(), 'POST_TELEMETRY_REQUEST');
  assert.strictEqual(a.metadata.itw_panel, 'ITW-DEEF3C');
  assert.strictEqual(a.msg.stay_id, '353:17');
  const h = histFn(a.msg, { ts: String(T) }, a.msgType);
  const pub = h.find(o => o.metadata.itw_out === 'PUBLISH');
  assert.strictEqual(pub.metadata.itw_client, 'incutwin-DEEF3C');
  assert.strictEqual(pub.msg.stays[0].holds, 1);
  assert.strictEqual(pub.msg.stays[0].days, 5);
  assert.strictEqual(pub.msg.stats.families, 1);
});

test('sync con bebe dentro / sin bebe', () => {
  const a = adapter('sync')({ kind: 'sync', panel: 'ITW-DEEF3C' }, incMd(), 'ATTRIBUTES_UPDATED');
  assert.strictEqual(a.msg.stay_id, '353:17');
  const b = adapter('sync')({ kind: 'sync', panel: 'ITW-DEEF3C' },
    incMd({ ss_itw_state: JSON.stringify({ v: 2, baby_state: 'out', stay_id: null }) }), 'ATTRIBUTES_UPDATED');
  assert.strictEqual(b.msg.stay_id, undefined);
});

let fail = 0;
for (const [n, f] of tests) {
  try { f(); console.log('PASS', n); } catch (e) { fail++; console.log('FAIL', n, '\n    ', e.message); }
}
console.log(`\n${tests.length - fail}/${tests.length} OK`);
process.exit(fail ? 1 : 0);
