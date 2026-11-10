// Escenarios de openspec/changes/tb-stay-history/specs/stay-history: node tb/test_history.js
// Ejecuta itw_history.js como lo hace ThingsBoard (cuerpo de funcion msg, metadata, msgType).
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const src = fs.readFileSync(path.join(__dirname, 'itw_history.js'), 'utf8');
const fn = new Function('msg', 'metadata', 'msgType', src);

const S = 1000, M = 60 * S, H = 60 * M, D = 24 * H;
const T0 = Date.parse('2026-11-02T09:00:00Z'); // lunes

let attr = null; // server attribute itw_history del panel
let offset = null; // itw_utc_offset_min del panel (null = por defecto)

function step(m, now) {
  const md = { deviceName: 'IncuNest-353', ts: String(now) }; // deviceName del mensaje original
  if (attr !== null) md.ss_itw_history = attr;
  if (offset !== null) md.ss_itw_utc_offset_min = String(offset);
  const input = Object.assign({ panel: 'ITW-DEEF3C', country: 'CM', show_name: true, name: '  Lucía  ', panel_count: 13 }, m);
  const out = fn(input, md, 'ITW_HISTORY');
  const save = out.find(o => o.metadata.itw_out === 'SAVE');
  const pub = out.find(o => o.metadata.itw_out === 'PUBLISH');
  assert.ok(save && pub, 'SAVE y PUBLISH');
  assert.strictEqual(save.msgType, 'POST_ATTRIBUTES_REQUEST');
  assert.strictEqual(pub.metadata.itw_client, 'incutwin-DEEF3C'); // topic del broker
  attr = save.msg.itw_history;
  return { stored: JSON.parse(attr), pub: pub.msg, raw: JSON.stringify(pub.msg) };
}

function reset() { attr = null; offset = null; }

const tests = [];
function test(name, f) { tests.push([name, f]); }

// ------------------------------------------------------------ historial de estancias

test('Llega un bebe', () => {
  reset();
  const r = step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  assert.strictEqual(r.pub.stays.length, 1);
  const s = r.pub.stays[0];
  assert.deepStrictEqual(
    { id: s.id, c: s.c, n: s.n, from: s.from, to: s.to, end: s.end, holds: s.holds, my_days: s.my_days },
    { id: '353:17', c: 'CM', n: 'Lucía', from: '2026-11-02', to: null, end: null, holds: 0, my_days: 0 });
  assert.strictEqual(r.pub.v, 1);
  assert.strictEqual(r.pub.tz, 60);
});

test('Vuelve del canguro: no duplica', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  const r = step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0 + 2 * H);
  assert.strictEqual(r.pub.stays.length, 1);
  assert.strictEqual(r.pub.stays[0].to, null);
});

test('Se va a casa', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  const r = step({ kind: 'stay_close', stay_id: '353:17', outcome: 'home', duration_days: 18, ts: T0 + 18 * D }, T0 + 18 * D);
  const s = r.pub.stays[0];
  assert.strictEqual(s.end, 'home');
  assert.strictEqual(s.to, '2026-11-20');
  assert.strictEqual(s.days, 18);
  assert.strictEqual(s.n, 'Lucía'); // a casa: conserva el nombre
  assert.strictEqual(r.pub.stats.families, 0); // ya no hay estancia en curso
});

test('Se va sin desenlace bueno: left y sin nombre', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  const r = step({ kind: 'stay_close', stay_id: '353:17', outcome: null, duration_days: 4.2, ts: T0 + 4 * D }, T0 + 4 * D);
  const s = r.pub.stays[0];
  assert.strictEqual(s.end, 'left');
  assert.strictEqual(s.n, '');
  assert.strictEqual(s.days, 4);
  assert.ok(!/Luc/.test(attr), 'el nombre no queda en el atributo');
});

test('Alta por error: se reabre la misma estancia', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  step({ kind: 'stay_close', stay_id: '353:17', outcome: null, duration_days: 1, ts: T0 + D }, T0 + D);
  const r = step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0 + D + 2 * H);
  assert.strictEqual(r.pub.stays.length, 1);
  assert.strictEqual(r.pub.stays[0].to, null);
  assert.strictEqual(r.pub.stays[0].end, null);
  assert.strictEqual(r.pub.stays[0].n, 'Lucía');
});

test('Mas de diez estancias: se descarta la mas antigua', () => {
  reset();
  for (let i = 1; i <= 11; i++) {
    const t = T0 + i * 3 * D;
    step({ kind: 'stay_open', stay_id: '353:' + i, in_since: t }, t);
    step({ kind: 'stay_close', stay_id: '353:' + i, outcome: 'home', duration_days: 2, ts: t + 2 * D }, t + 2 * D);
  }
  const r = step({ kind: 'sync' }, T0 + 40 * D);
  assert.strictEqual(r.stored.stays.length, 10);
  assert.strictEqual(r.stored.stays[0].id, '353:11'); // la mas reciente primero
  assert.ok(!r.stored.stays.some(s => s.id === '353:1'));
});

// ------------------------------------------------------------ Coge mi mano

test('Dos en el mismo dia: holds +2, my_days +1', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + 1 * H);
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + 3 * H);
  assert.strictEqual(r.pub.stays[0].holds, 2);
  assert.strictEqual(r.pub.stays[0].my_days, 1);
  assert.strictEqual(r.pub.stats.days, 1);
});

test('Hold de una estancia anterior al despliegue: se crea con in_since', () => {
  reset();
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 - 5 * D }, T0);
  const s = r.pub.stays[0];
  assert.strictEqual(s.from, '2026-10-28');
  assert.strictEqual(s.holds, 1);
  assert.strictEqual(s.days, 5);
});

// ------------------------------------------------------------ logros

test('Racha: lunes, martes, miercoles', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0);
  step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + D);
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + 2 * D);
  assert.strictEqual(r.pub.stats.streak, 3);
  assert.strictEqual(r.pub.stats.days, 3);
  assert.strictEqual(r.pub.stats.last, '2026-11-04');
  // jueves sin ninguno: sigue en 3; viernes: 0
  assert.strictEqual(step({ kind: 'sync', stay_id: '353:17', in_since: T0 }, T0 + 3 * D).pub.stats.streak, 3);
  assert.strictEqual(step({ kind: 'sync', stay_id: '353:17', in_since: T0 }, T0 + 4 * D).pub.stats.streak, 0);
});

test('Racha que se corta y vuelve a empezar', () => {
  reset();
  step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0);
  step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + D);
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0 + 4 * D);
  assert.strictEqual(r.pub.stats.streak, 1);
  assert.strictEqual(r.pub.stats.days, 3);
});

test('Dias acompanados suman todas las estancias', () => {
  reset();
  step({ kind: 'hold', stay_id: '353:16', in_since: T0 }, T0);
  step({ kind: 'stay_close', stay_id: '353:16', outcome: 'home', duration_days: 1, ts: T0 + 2 * H }, T0 + 2 * H);
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 + D }, T0 + D);
  assert.strictEqual(r.pub.stats.days, 2);
  assert.strictEqual(r.pub.stats.streak, 2);
});

test('Dia local con el desplazamiento del panel', () => {
  reset();
  // 23:30 UTC del lunes = 00:30 del martes en UTC+1
  const late = Date.parse('2026-11-02T23:30:00Z');
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: late }, late);
  assert.strictEqual(r.pub.stats.last, '2026-11-03');
  reset();
  offset = 0; // mismo instante en un panel en UTC: sigue siendo lunes
  const r2 = step({ kind: 'hold', stay_id: '353:17', in_since: late }, late + M);
  assert.strictEqual(r2.pub.tz, 0);
  assert.strictEqual(r2.pub.stats.last, '2026-11-02');
});

test('Familias: itw_panel_count - 1', () => {
  reset();
  const r = step({ kind: 'stay_open', stay_id: '353:17', in_since: T0, panel_count: 13 }, T0);
  assert.strictEqual(r.pub.stats.families, 12);
  const r2 = step({ kind: 'sync', stay_id: '353:17', in_since: T0, panel_count: 1 }, T0 + H);
  assert.strictEqual(r2.pub.stats.families, 0);
});

// ------------------------------------------------------------ entrega

test('Mensaje <= 1000 bytes; el atributo conserva 10', () => {
  reset();
  for (let i = 1; i <= 10; i++) {
    const t = T0 + i * 3 * D;
    step({ kind: 'stay_open', stay_id: 'INC-LARGO-' + i + ':' + (1000 + i), in_since: t, name: 'NombreLargoDeBebé' + i }, t);
    step({ kind: 'hold', stay_id: 'INC-LARGO-' + i + ':' + (1000 + i), in_since: t, name: 'NombreLargoDeBebé' + i }, t + H);
    step({ kind: 'stay_close', stay_id: 'INC-LARGO-' + i + ':' + (1000 + i), outcome: 'home', duration_days: 2, ts: t + 2 * D }, t + 2 * D);
  }
  const r = step({ kind: 'sync' }, T0 + 40 * D);
  assert.ok(Buffer.byteLength(r.raw, 'utf8') <= 1000, 'bytes=' + Buffer.byteLength(r.raw, 'utf8'));
  assert.strictEqual(r.stored.stays.length, 10);
  assert.ok(r.pub.stays.length < 10 && r.pub.stays.length >= 1);
  assert.strictEqual(r.pub.stays[0].id, 'INC-LARGO-10:1010'); // quita las mas antiguas
});

test('El mensaje no lleva datos internos', () => {
  reset();
  const r = step({ kind: 'hold', stay_id: '353:17', in_since: T0 }, T0);
  assert.ok(!('hold_days' in r.pub));
  assert.ok(!('ld' in r.pub.stays[0]));
  assert.ok(typeof r.pub.upd === 'number');
});

// ------------------------------------------------------------ privacidad

test('Sin consentimiento: nombre vacio', () => {
  reset();
  const r = step({ kind: 'stay_open', stay_id: '353:17', in_since: T0, show_name: false }, T0);
  assert.strictEqual(r.pub.stays[0].n, '');
  assert.ok(!/Luc/.test(attr));
});

test('Fallecimiento o traslado: left sin nombre, como cualquier otro', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  // el puente solo pasa outcome 'home' o null: un 2 (fallece) llega como null
  const r = step({ kind: 'stay_close', stay_id: '353:17', outcome: 2, duration_days: 3, ts: T0 + 3 * D }, T0 + 3 * D);
  assert.strictEqual(r.pub.stays[0].end, 'left');
  assert.strictEqual(r.pub.stays[0].n, '');
  assert.ok(!/"outcome"|"reason"/.test(attr));
});

// ------------------------------------------------------------ emparejamiento

test('Emparejar con un bebe dentro desde hace 5 dias', () => {
  reset();
  const r = step({ kind: 'sync', stay_id: '353:17', in_since: T0 - 5 * D, panel_count: 2 }, T0);
  assert.strictEqual(r.pub.stays.length, 1);
  assert.strictEqual(r.pub.stays[0].days, 5);
  assert.strictEqual(r.pub.stays[0].holds, 0);
  assert.strictEqual(r.pub.stats.families, 1);
});

test('Sync sin estancia en curso: no crea nada', () => {
  reset();
  const r = step({ kind: 'sync' }, T0);
  assert.strictEqual(r.pub.stays.length, 0);
  assert.strictEqual(r.pub.stats.families, 0);
});

test('Desemparejar: la estancia queda acompanada hasta hoy', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  const r = step({ kind: 'unpaired' }, T0 + 2 * D);
  const s = r.pub.stays[0];
  assert.strictEqual(s.to, '2026-11-04');
  assert.strictEqual(s.end, null);
  assert.strictEqual(r.pub.stats.families, 0);
  // un baby_out posterior de esa IncuNest ya no le llega; si llegara, no la cambia
  const r2 = step({ kind: 'stay_close', stay_id: '353:17', outcome: 'home', duration_days: 9, ts: T0 + 9 * D }, T0 + 9 * D);
  assert.strictEqual(r2.pub.stays[0].to, '2026-11-04');
});

test('Entrada invalida: no rompe el historial', () => {
  reset();
  step({ kind: 'stay_open', stay_id: '353:17', in_since: T0 }, T0);
  const before = attr;
  const r = step({ kind: 'stay_open' }, T0 + H); // sin stay_id
  assert.strictEqual(r.pub.stays.length, 1);
  attr = '{roto'; // atributo corrupto: empieza de cero
  const r2 = step({ kind: 'sync' }, T0 + 2 * H);
  assert.strictEqual(r2.pub.stays.length, 0);
  assert.ok(before.length > 0);
});

// ------------------------------------------------------------ runner

let fail = 0;
for (const [name, f] of tests) {
  try { f(); console.log('PASS', name); } catch (e) { fail++; console.log('FAIL', name, '\n    ', e.message); }
}
console.log(`\n${tests.length - fail}/${tests.length} OK`);
process.exit(fail ? 1 : 0);
