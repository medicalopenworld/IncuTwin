// Identidad del bebé (sexo y nombre) en itw_state_machine.js — Contrato v0.2 §3.4.1.
// Uso: node tb/test_identity.js   (sale con código != 0 si algo falla)
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const src = fs.readFileSync(path.join(__dirname, 'itw_state_machine.js'), 'utf8');
const fn = new Function('msg', 'metadata', 'msgType', src);
const NAMES = new Function(src.slice(src.indexOf('var NAMES'), src.indexOf('// FNV-1a')) + 'return NAMES;')();

const S = 1000, M = 60 * S, H = 60 * M;
const T0 = Date.parse('2026-11-24T08:00:00Z');

// Una incubadora simulada: guarda itw_state y los client attributes como haría ThingsBoard.
function incubator(sn, extraMd) {
  const inc = { sn, state: null, cs: {}, last: null, broker: null, t: T0 };
  inc.step = (msg, msgType, dt) => {
    inc.t += dt === undefined ? 5 * S : dt;
    const md = Object.assign({ deviceName: 'IncuNest-' + sn, SN: sn, ts: String(inc.t) }, extraMd || {});
    for (const k in inc.cs) md['cs_' + k] = String(inc.cs[k]);
    if (inc.state) md.ss_itw_state = inc.state;
    const out = fn(msg, md, msgType);
    if (msgType === 'POST_ATTRIBUTES_REQUEST') Object.assign(inc.cs, msg); // TB los guarda después
    inc.state = out.find(o => o.metadata.itw_out === 'ITW_STATE').msg.itw_state;
    inc.attrs = out.find(o => o.metadata.itw_out === 'ITW_STATE').msg;
    inc.panel = out.find(o => o.metadata.itw_out === 'ITW_PANEL').msg;
    const b = out.find(o => o.metadata.itw_out === 'ITW_BROKER');
    if (b) inc.broker = b.msg;
    inc.events = out.filter(o => o.metadata.itw_out === 'ITW_EVENT').map(o => o.msg);
    return inc;
  };
  inc.attrsMsg = (a) => inc.step(a, 'POST_ATTRIBUTES_REQUEST');
  inc.tele = (m, dt) => inc.step(m, 'POST_TELEMETRY_REQUEST', dt);
  // la enfermera crea el perfil y enciende el calor
  inc.admit = (seq, extra) => {
    inc.attrsMsg(Object.assign({ baby_seq: seq, baby_admission_epoch: Math.floor(inc.t / 1000) }, extra || {}));
    return inc.tele({ Control_active: true, Control_mode: 'AIR', Temp_desired: 36, Air_temp: 30, Phototherapy_active: false });
  };
  return inc;
}
const isName = (sex, n) => NAMES[sex].indexOf(n) >= 0;
let n = 0;
function test(title, body) { body(); n++; console.log('ok', title); }

test('las listas de apodos caben en el panel y solo usan glifos de sus fuentes', () => {
  for (const sex of ['f', 'm']) {
    assert.ok(NAMES[sex].length >= 20);
    assert.strictEqual(new Set(NAMES[sex]).size, NAMES[sex].length, 'apodo repetido');
    for (const name of NAMES[sex]) {
      assert.ok(Buffer.byteLength(name, 'utf8') <= 23, name);
      assert.ok(/^[A-Za-zÁÉÍÓÚÑÜáéíóúñü]+$/.test(name), name);
    }
  }
  assert.strictEqual(NAMES.f.filter(x => NAMES.m.indexOf(x) >= 0).length, 0, 'apodo en las dos listas');
});

test('bebé nuevo sin sexo: el sistema sortea sexo y apodo y los publica en el broker', () => {
  const a = incubator('353').admit(6);
  assert.strictEqual(a.events[0].event, 'baby_in');
  assert.ok(a.broker.sex === 'm' || a.broker.sex === 'f');
  assert.ok(isName(a.broker.sex, a.broker.name), a.broker.name);
  assert.strictEqual(a.panel.sex, a.broker.sex);
  assert.strictEqual(a.panel.name, a.broker.name);
  assert.strictEqual(a.attrs.itw_baby_sex, a.broker.sex);
  assert.strictEqual(a.attrs.itw_baby_sex_src, 'auto');
  assert.strictEqual(a.attrs.itw_baby_alias, a.broker.name);
  assert.strictEqual(a.broker.weight_g, undefined, 'el peso no sale sin consentimiento');
  assert.strictEqual(a.broker.age_d, undefined);
});

test('sin bebé no hay nombre ni sexo', () => {
  const a = incubator('353');
  a.attrsMsg({ baby_seq: 5, baby_admission_epoch: 0 });      // perfil viejo en el slot, sin terapia
  a.tele({ Control_active: false, Phototherapy_active: false });
  assert.strictEqual(a.panel.baby, 'none');
  assert.strictEqual(a.panel.name, '');
  assert.strictEqual(a.panel.sex, '');
  assert.strictEqual(a.attrs.itw_baby_sex, '');
});

test('la identidad no cambia durante el episodio (latidos, canguro, vuelta, offline/online)', () => {
  const a = incubator('353').admit(6);
  const id = [a.broker.name, a.broker.sex];
  const same = () => assert.deepStrictEqual([a.panel.name, a.panel.sex], id);
  for (let i = 0; i < 30; i++) { a.tele({ Control_active: true, Air_temp: 35, HR1: 140, HR1_SQI: 0.9 }); same(); }
  a.step({}, 'INACTIVITY_EVENT', 3 * M); same();
  a.step({}, 'ACTIVITY_EVENT', 10 * M); same();
  a.tele({ Control_active: false, Phototherapy_active: false }, 10 * M);
  a.tele({ baby_seq: 6, baby_kangaroo_event: 1, baby_kangaroo_count: 1 });
  assert.strictEqual(a.panel.baby, 'parents'); same();
  a.tele({ Control_active: true, Air_temp: 34 }, 1 * H);
  assert.strictEqual(a.panel.baby, 'in'); same();
  assert.deepStrictEqual([a.broker.name, a.broker.sex], id);
});

test('alta a casa: el panel sigue recibiendo el mismo nombre; el siguiente bebé estrena identidad', () => {
  const a = incubator('353').admit(6);
  const first = [a.broker.name, a.broker.sex];
  a.tele({ Control_active: false, Phototherapy_active: false }, 2 * 24 * H);
  a.tele({ baby_seq: 6, baby_outcome: 1, baby_discharge_epoch: Math.floor(a.t / 1000) });
  a.attrsMsg({ baby_seq: 0, baby_admission_epoch: 0 });
  assert.strictEqual(a.broker.baby, 'home');
  assert.deepStrictEqual([a.panel.name, a.panel.sex], first);
  a.admit(7);
  assert.strictEqual(a.events[0].event, 'baby_in');
  assert.notStrictEqual(a.broker.name, first[0], 'dos bebés seguidos no comparten apodo');
  assert.ok(isName(a.broker.sex, a.broker.name));
});

test('reemplazo sin alta (baby_seq nuevo bajo terapia) = bebé nuevo', () => {
  const a = incubator('353').admit(6);
  const first = a.broker.name;
  // con la terapia encendida, el cambio de perfil ya cierra un episodio y abre el otro
  a.attrsMsg({ baby_seq: 7, baby_admission_epoch: Math.floor(a.t / 1000) });
  assert.deepStrictEqual(a.events.map(e => e.event).slice(0, 2), ['baby_out', 'baby_in']);
  assert.strictEqual(a.events[0].payload.reason, 'replaced');
  assert.strictEqual(a.attrs.itw_stay_id, '353:7');
  assert.notStrictEqual(a.broker.name, first);
  assert.ok(isName(a.broker.sex, a.broker.name));
});

test('alta por error: reabrir el mismo baby_seq conserva la identidad', () => {
  const a = incubator('353').admit(6);
  const id = [a.broker.name, a.broker.sex];
  a.tele({ Control_active: false, Phototherapy_active: false }, 1 * H);
  a.tele({ baby_seq: 6, baby_outcome: 0, baby_discharge_epoch: Math.floor(a.t / 1000) });
  assert.strictEqual(a.panel.baby, 'out');
  assert.strictEqual(a.panel.name, '', 'alta que no es a casa: el bebé deja de mostrarse');
  a.tele({ Control_active: true, Air_temp: 34 }, 1 * H);
  assert.strictEqual(a.events[0].event, 'baby_back');
  assert.deepStrictEqual([a.broker.name, a.broker.sex], id);
});

test('reproducible: perder itw_state a mitad del episodio no cambia al bebé', () => {
  const a = incubator('353').admit(6);
  const id = [a.broker.name, a.broker.sex];
  a.state = null;                                            // TB pierde el server attribute
  a.tele({ Control_active: true, Air_temp: 35 });
  assert.deepStrictEqual([a.panel.name, a.panel.sex], id);
  const b = incubator('353').admit(6);                       // otra instancia de TB, mismo stay_id
  assert.deepStrictEqual([b.broker.name, b.broker.sex], id);
});

test('el mismo baby_seq en otra incubadora es otro bebé (el hash es del stay_id)', () => {
  let differ = 0;
  for (let seq = 1; seq <= 40; seq++) {
    const a = incubator('353').admit(seq), b = incubator('354').admit(seq);
    if (a.broker.name !== b.broker.name) differ++;
  }
  assert.ok(differ >= 35, 'coinciden demasiado: ' + differ);
});

test('si la IncuNest manda el sexo, se respeta (ISO 5218 y m/f)', () => {
  const cases = [[1, 'm'], [2, 'f'], ['1', 'm'], ['2', 'f'], ['M', 'm'], ['f', 'f'], ['male', 'm'], ['Female', 'f']];
  for (let seq = 1; seq <= 24; seq++) {
    const [raw, want] = cases[seq % cases.length];
    const a = incubator('353').admit(seq, { baby_sex: raw });
    assert.strictEqual(a.broker.sex, want, 'baby_sex=' + raw);
    assert.ok(isName(want, a.broker.name), a.broker.name);
    assert.strictEqual(a.attrs.itw_baby_sex_src, 'device');
  }
});

test('sexo 0, vacío o desconocido = no ha llegado: se sortea', () => {
  for (const raw of [0, '', 9, 'x', '0']) {
    const a = incubator('353').admit(6, { baby_sex: raw });
    const ref = incubator('353').admit(6);
    assert.strictEqual(a.attrs.itw_baby_sex_src, 'auto', 'baby_sex=' + JSON.stringify(raw));
    assert.deepStrictEqual([a.broker.name, a.broker.sex], [ref.broker.name, ref.broker.sex]);
  }
});

test('el sexo real que llega tarde corrige el sorteado (y el apodo, que va con el sexo)', () => {
  let seq = 1, a;
  for (;; seq++) { a = incubator('353').admit(seq); if (a.broker.sex === 'm') break; }
  const before = a.broker.name;
  a.attrsMsg({ baby_sex: 2 });
  assert.strictEqual(a.panel.sex, 'f');
  assert.ok(isName('f', a.panel.name));
  assert.strictEqual(a.attrs.itw_baby_sex_src, 'device');
  // si confirma el sorteado, no cambia nada salvo el origen
  const b = incubator('353').admit(seq);
  b.attrsMsg({ baby_sex: 1 });
  assert.deepStrictEqual([b.panel.name, b.panel.sex], [before, 'm']);
  assert.strictEqual(b.attrs.itw_baby_sex_src, 'device');
});

test('el sexo del bebé anterior no se hereda', () => {
  const a = incubator('353').admit(6, { baby_sex: 2 });
  assert.strictEqual(a.attrs.itw_baby_sex_src, 'device');
  a.attrsMsg({ baby_seq: 7, baby_admission_epoch: Math.floor(a.t / 1000), baby_sex: 0 });
  a.tele({ Control_active: true, Air_temp: 33 });
  assert.strictEqual(a.attrs.itw_stay_id, '353:7');
  assert.strictEqual(a.attrs.itw_baby_sex_src, 'auto');
});

test('nombre real solo con itw_show_name; el sexo sigue saliendo del mismo sitio', () => {
  const md = { ss_itw_show_name: 'true' };
  const a = incubator('353', md);
  a.admit(6, { baby_name: '  Amara  ', baby_weight_g: 1250 });
  assert.strictEqual(a.broker.name, 'Amara');
  assert.strictEqual(a.broker.weight_g, 1250);
  assert.strictEqual(a.broker.age_d, 0);
  assert.ok(isName(a.broker.sex, a.attrs.itw_baby_alias), 'el apodo se guarda aunque no se muestre');
  const b = incubator('353').admit(6, { baby_name: 'Amara', baby_weight_g: 1250 });
  assert.notStrictEqual(b.broker.name, 'Amara', 'sin consentimiento el nombre real no sale');
  assert.strictEqual(b.broker.sex, a.broker.sex);
  // consentimiento pero la enfermera no escribió nombre: apodo
  const c = incubator('353', md).admit(6, { baby_name: '' });
  assert.strictEqual(c.broker.name, b.broker.name);
});

test('el nombre real se recorta a 23 bytes UTF-8 sin partir caracteres', () => {
  const a = incubator('353', { ss_itw_show_name: 'true' }).admit(6, { baby_name: 'ÁÉÍÓÚÑáéíóúñÁÉÍÓÚÑáé' });
  assert.ok(Buffer.byteLength(a.broker.name, 'utf8') <= 23);
  assert.strictEqual(a.broker.name, 'ÁÉÍÓÚÑáéíóú');
});

test('tras el alta, el nombre real de otro perfil no se cuela en el bebé que se fue a casa', () => {
  const a = incubator('353', { ss_itw_show_name: 'true' }).admit(6, { baby_name: 'Amara' });
  a.tele({ Control_active: false, Phototherapy_active: false }, 1 * H);
  a.tele({ baby_seq: 6, baby_outcome: 1, baby_discharge_epoch: Math.floor(a.t / 1000) });
  assert.strictEqual(a.panel.baby, 'out');
  assert.strictEqual(a.panel.name, 'Amara');
  a.attrsMsg({ baby_seq: 5, baby_name: 'Otro', baby_admission_epoch: 0 }); // la IncuNest muestra un perfil antiguo
  a.tele({ Control_active: false, Phototherapy_active: false });
  assert.strictEqual(a.panel.baby, 'out');
  assert.strictEqual(a.panel.name, a.attrs.itw_baby_alias);
});

test('el sorteo es equilibrado y no alterna con baby_seq consecutivos', () => {
  let f = 0, flips = 0, prev = null, total = 0;
  const used = new Set();
  for (const sn of ['1', '353', 'IN3-2601-0042', 'TEST-001']) {
    prev = null;
    for (let seq = 1; seq <= 250; seq++, total++) {
      const a = incubator(sn).admit(seq);
      if (a.broker.sex === 'f') f++;
      if (prev !== null && prev !== a.broker.sex) flips++;
      prev = a.broker.sex; used.add(a.broker.name);
    }
  }
  assert.ok(f > total * 0.42 && f < total * 0.58, 'niñas: ' + f + '/' + total);
  assert.ok(flips > total * 0.38 && flips < total * 0.62, 'cambios consecutivos: ' + flips + '/' + total);
  assert.strictEqual(used.size, NAMES.f.length + NAMES.m.length, 'apodos que nunca salen');
});

test('el payload del broker sigue cabiendo en 512 bytes', () => {
  const a = incubator('IN3-2601-0042', { ss_itw_show_name: 'true' });
  a.admit(123456, { baby_name: 'ÁÉÍÓÚÑáéíóúñÁÉÍÓÚÑáé', baby_weight_g: 4999, baby_sex: 2 });
  a.tele({ Control_active: true, Air_temp: 36, Temp_desired: 36, Phototherapy_active: true, HR1: 180, HR1_SQI: 0.9, SpO2_SQI: 1 }, 2 * M);
  assert.ok(Buffer.byteLength(JSON.stringify(a.broker), 'utf8') <= 512);
});

console.log('\n' + n + ' pruebas OK');
