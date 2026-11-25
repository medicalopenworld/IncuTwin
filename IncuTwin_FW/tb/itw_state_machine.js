// ThingsBoard rule node: Transformation script (JS)
// Rama "estado del gemelo" — Contrato de eventos IncuTwin v0.2 §3.4
// Entrada: telemetría/atributos/actividad de una IncuNest con incutwin_enabled=true.
// Regla: termorregulación y/o fototerapia => hay bebé. Sin bebé registrado (baby_seq=0) se abre
// una estancia anónima (stay_seq 0, stay_id SN:0-<epoch>) sin nombre/peso/días; si después se
// registra, la estancia adopta ese baby_seq sin cerrarse.
// metadata.ss_itw_state       -> estado previo (JSON) fetched by "Originator attributes"
// metadata.ss_itw_t_parents_min, ss_itw_t_idle_min -> timeouts opcionales por incubadora
// metadata.SN                 -> último valor de telemetría SN (fallback: deviceName)
// metadata.Control_active, Phototherapy_active, Control_mode, Temp_desired -> últimos valores
//                                guardados; se usan si el mensaje no los trae (llegan al cambiar)
// metadata.ss_itw_show_name   -> 'true': el panel emparejado muestra nombre, peso de ingreso
//                                y días desde el ingreso del bebé
//                                (excepción consentida al "nada de nombres sale de TB";
//                                nunca va en los eventos del webhook)
// metadata.ss_country         -> país ISO alfa-2 de la incubadora (lo pone MOW); va al broker
// metadata.countryCode / msg.countryCode -> el que manda la IncuNest por telemetría
//                                (geolocalización); se usa si no hay ss_country. "XX" = desconocido
// metadata.cs_baby_*          -> client attributes del bebé ya guardados (sincroniza el
//                                estado aunque la chain se despliegue con la incubadora ya en marcha)
// metadata.cs_baby_sex        -> sexo del bebé si la IncuNest lo publica (ISO 5218: 1 niño, 2 niña;
//                                también 'm'/'f'). Ausente o 0 = no se sabe: lo decide esta chain.
//
// Identidad del bebé (Contrato v0.2 §3.4.1). Todo bebé que se muestra lleva nombre y sexo, y se
// deciden AQUÍ, una vez por episodio, para que todos los paneles de la incubadora reciban lo mismo:
//  - bebé nuevo  = stay_id nuevo (SN:baby_seq). Canguro, reapertura, cortes de red y reinicios
//                  conservan la identidad.
//  - sexo        = el de la IncuNest si llega; si no, al azar. El "azar" es un hash del stay_id:
//                  reproducible aunque se pierda itw_state, igual para todos los paneles.
//  - nombre      = baby_name real solo con itw_show_name (consentimiento); si no, ninguno: el
//                  panel pregunta y guarda el suyo por stay_id (que va en el broker).
//  - apodo       = de la lista del sexo elegido, por hash del stay_id; solo en itw_baby_alias y
//                  en los eventos del webhook, nunca al panel.
// Salida: [ITW_STATE, ITW_PANEL, ITW_EVENT*]

// Hora del mensaje (la IncuNest manda telemetría atrasada con su propio ts), pero nunca por delante
// del servidor: un reloj de IncuNest adelantado (353, 25-nov-2026: +1 h 42 min) dejaba fechas
// futuras en el estado y frenaba el debounce de tratamientos y el heartbeat hasta alcanzarlas.
var srvNow = new Date().getTime();
var now = metadata.ts ? Math.min(parseInt(metadata.ts) || srvNow, srvNow) : srvNow;
var incubator = metadata.SN ? String(metadata.SN) : String(metadata.deviceName || metadata.originatorName || 'unknown');

var st = null;
try { if (metadata.ss_itw_state) st = JSON.parse(metadata.ss_itw_state); } catch (e) { st = null; }
if (!st || st.v !== 2) {
  st = { v: 2, seq: 0, online: true, last_seen: 0,
    control_active: false, photo: false, spo2: false, hr: null, hr_q: 'none',
    control_mode: null, temp_desired: null, stable_since: null, heat: 'off',
    baby_seq: 0, admission_epoch: 0, kangaroo_count: 0, thermo_min: 0, photo_min: 0,
    baby_state: 'none', stay_seq: 0, stay_id: null, in_since: null, idle_since: null,
    parents_since: null, out_since: null, out_seq: 0, last_hb: 0, last_tx: null, last_tx_change: 0, care: false };
}
// estados guardados con la versión anterior: fechas futuras (ver srvNow) y las alarmas, que ya no
// se usan (IncuTwin nunca enseña alarmas)
var TIMES = ['last_seen', 'stable_since', 'in_since', 'idle_since', 'parents_since', 'out_since'];
for (var ti = 0; ti < TIMES.length; ti++) { if (st[TIMES[ti]] && st[TIMES[ti]] > srvNow) { st[TIMES[ti]] = now; } }
if (st.last_hb > srvNow) { st.last_hb = 0; }               // que el próximo latido salga ya
if (st.last_tx_change > srvNow) { st.last_tx_change = 0; } // y el cambio de tratamiento pendiente
delete st.alarms;
if (st.heat === 'alarm') { st.heat = st.control_active ? 'heating' : 'off'; }

var cfg = {
  t_parents_ms: (parseInt(metadata.ss_itw_t_parents_min) || 240) * 60000,
  t_idle_ms:    (parseInt(metadata.ss_itw_t_idle_min)    || 360) * 60000,
  hb_ms: 60000, tx_debounce_ms: 30000, reopen_ms: 24 * 3600000, stable_ms: 300000
};

var events = [];
function isoCountry(v) {
  var c = String(v || '').toUpperCase();
  return (/^[A-Z]{2}$/.test(c) && c !== 'XX') ? c : '';
}
function lastOf(k) {
  if (msg[k] !== undefined) return msg[k];
  return (metadata[k] !== undefined && metadata[k] !== '') ? metadata[k] : undefined;
}
function countryOf() {
  return isoCountry(metadata.ss_country) || isoCountry(msg && msg.countryCode) || isoCountry(metadata.countryCode);
}
function iso(ms) { return new Date(ms).toISOString(); }
function uuid() {
  var s = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx';
  return s.replace(/[xy]/g, function (c) { var r = Math.floor(Math.random() * 16); return (c === 'x' ? r : (r & 3 | 8)).toString(16); });
}
function truthy(v) { return v === true || v === 'true' || v === 1 || v === '1'; }
function num(v) { var n = parseFloat(v); return isNaN(n) ? null : n; }
function tx() { return { heat: st.heat, photo: st.photo, spo2: st.spo2 }; }
function emit(type, payload, ts) {
  st.seq++;
  events.push({ schema: 'incutwin.events/0.2', event_id: uuid(), event: type, seq: st.seq,
    ts: iso(ts || now), received_at: iso(new Date().getTime()),
    incubator_id: incubator, stay_id: st.stay_id, payload: payload });
}
function closeStay(reason, outcome, ts) {
  var t = ts || now;
  var dur = st.in_since ? Math.round(((t - st.in_since) / 86400000) * 10) / 10 : null;
  emit('baby_out', { reason: reason, duration_days: dur,
    summary: { thermo_h: Math.round(st.thermo_min / 6) / 10, photo_h: Math.round(st.photo_min / 6) / 10, kangaroo: st.kangaroo_count },
    outcome: (reason === 'discharged' && outcome === 1) ? 'home' : null }, t);
  st.baby_state = 'out'; st.out_since = t; st.out_seq = st.stay_seq; st.out_stay_id = st.stay_id;
  st.out_home = (reason === 'discharged' && outcome === 1);
  st.stay_id = null; st.in_since = null; st.idle_since = null; st.parents_since = null;
}
function openStay(seq, ts) {
  var t = ts || now;
  var reopen = (st.out_seq === seq && st.out_since && (t - st.out_since) < cfg.reopen_ms);
  if (!reopen) { st.thermo_min = 0; st.photo_min = 0; st.kangaroo_count = 0; }
  st.stay_seq = seq; st.baby_state = 'in'; st.out_home = false;
  if (seq > 0) st.stay_id = incubator + ':' + seq;
  else st.stay_id = (reopen && st.out_stay_id) ? st.out_stay_id : incubator + ':0-' + Math.floor(t / 1000);
  st.in_since = reopen && st.in_since ? st.in_since : t; st.idle_since = null; st.parents_since = null;
  syncIdentity();  // antes de emitir: baby_in ya lleva apodo y sexo
  var p = idPayload(tx());
  if (reopen) { p.away_min = Math.round((t - st.out_since) / 60000); emit('baby_back', p, t); }
  else { p.admitted_at = (seq > 0 && st.admission_epoch) ? iso(st.admission_epoch * 1000) : null; emit('baby_in', p, t); }
  st.last_tx = JSON.stringify(tx()); st.last_tx_change = t;
}

// ---- identidad del bebé: sexo y nombre ----
// Solo ASCII + áéíóúñü: son los glifos de las fuentes del panel (tools/gen_fonts.py). <= 23 bytes UTF-8.
var NAMES = {
  f: ['Lucía', 'Alma', 'Sara', 'Nora', 'Vera', 'Emma', 'Julia', 'Clara', 'Elena', 'Carla',
      'Irene', 'Paula', 'Marta', 'Daniela', 'Valeria', 'Aitana', 'Noa', 'Mía', 'Zoe', 'Abril',
      'Luna', 'Olivia', 'Martina', 'Sofía', 'Inés', 'Laia', 'Amina', 'Zuri', 'Nala', 'Amara',
      'Aisha', 'Imani', 'Kira', 'Maya', 'Lía', 'Ana', 'Eva', 'Iris', 'Leila', 'Naima'],
  m: ['Hugo', 'Leo', 'Mateo', 'Lucas', 'Martín', 'Daniel', 'Pablo', 'Adrián', 'Bruno', 'Enzo',
      'Marco', 'Darío', 'Izan', 'Thiago', 'Gael', 'Nico', 'Samuel', 'David', 'Diego', 'Iván',
      'Oliver', 'Liam', 'Noé', 'Omar', 'Amir', 'Kofi', 'Tariq', 'Malik', 'Idris', 'Samir',
      'Elías', 'Tomás', 'Andrés', 'Luis', 'Juan', 'Raúl', 'Rubén', 'Simón', 'Jamal', 'Yusuf']
};
// FNV-1a de 32 bits + una vuelta de xorshift (ES5, sin Math.imul). El bit bajo de FNV solo depende
// de la paridad de la entrada: sin la mezcla final, baby_seq consecutivos alternarían niño/niña.
function hash32(s) {
  var h = 0x811c9dc5;
  for (var c = 0; c < s.length; c++) {
    h ^= s.charCodeAt(c);
    h = (h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24))) >>> 0;
  }
  h ^= h << 13; h >>>= 0; h ^= h >>> 17; h ^= h << 5; h >>>= 0;
  h ^= h << 13; h >>>= 0; h ^= h >>> 17; h ^= h << 5; h >>>= 0;
  return h;
}
function parseSex(v) {
  if (v === undefined || v === null) return null;
  var s = String(v).trim().toLowerCase();
  if (s === 'm' || s === 'male' || s === 'boy' || s === '1') return 'm';
  if (s === 'f' || s === 'female' || s === 'girl' || s === '2') return 'f';
  return null;
}
function identify(stayId, devSex) {
  var prev = st.id, h = hash32(stayId);
  var sex = devSex || (((h >>> 16) & 1) ? 'f' : 'm');
  var list = NAMES[sex], idx = (h >>> 4) % list.length;
  // dos bebés seguidos en la misma incubadora no comparten apodo
  if (prev && prev.stay !== stayId && prev.alias === list[idx]) idx = (idx + 1) % list.length;
  st.id = { stay: stayId, sex: sex, src: devSex ? 'device' : 'auto', alias: list[idx] };
}
// Fija la identidad del episodio abierto o la revisa. true = cambió una identidad ya publicada
// (la IncuNest informa tarde de un sexo distinto del sorteado: el dato real manda).
function syncIdentity() {
  var devSex = (st.baby_seq === st.stay_seq) ? (st.baby_sex || null) : null;
  if (!st.id || st.id.stay !== st.stay_id) { identify(st.stay_id, devSex); return false; }
  if (devSex && devSex !== st.id.sex) { identify(st.stay_id, devSex); return true; }
  if (devSex) st.id.src = 'device';
  return false;
}
// Identidad en los eventos del webhook: SIEMPRE el apodo, nunca baby_name (§0.4), aunque el
// panel muestre el nombre real con itw_show_name.
function idPayload(p) { p.alias = st.id.alias; p.sex = st.id.sex; p.sex_src = st.id.src; return p; }
// recorta a `max` bytes UTF-8 sin partir un carácter (el panel descarta nombres de más de 23)
function clipUtf8(s, max) {
  var out = '', n = 0;
  for (var c = 0; c < s.length; c++) {
    var code = s.charCodeAt(c), pair = code >= 0xD800 && code <= 0xDBFF && c + 1 < s.length;
    var len = code < 0x80 ? 1 : code < 0x800 ? 2 : pair ? 4 : 3;
    if (n + len > max) break;
    out += pair ? s.substring(c, c + 2) : s.charAt(c);
    n += len; if (pair) c++;
  }
  return out;
}


// client attributes ya guardados: la fuente de verdad del episodio aunque el mensaje no los traiga
if (metadata.cs_baby_seq !== undefined && msgType !== 'POST_ATTRIBUTES_REQUEST') {
  st.baby_seq = parseInt(metadata.cs_baby_seq) || 0;
  st.baby_sex = parseSex(metadata.cs_baby_sex);
  if (metadata.cs_baby_admission_epoch !== undefined) st.admission_epoch = parseInt(metadata.cs_baby_admission_epoch) || 0;
  if (st.baby_state === 'in' || st.baby_state === 'parents') {
    if (metadata.cs_baby_kangaroo_count !== undefined) st.kangaroo_count = parseInt(metadata.cs_baby_kangaroo_count) || st.kangaroo_count;
    if (metadata.cs_baby_thermo_min !== undefined) st.thermo_min = parseInt(metadata.cs_baby_thermo_min) || st.thermo_min;
    if (metadata.cs_baby_phototherapy_min !== undefined) st.photo_min = parseInt(metadata.cs_baby_phototherapy_min) || st.photo_min;
  }
}

if (msgType === 'INACTIVITY_EVENT') {
  if (st.online) { st.online = false; emit('incubator_offline', { last_seen: st.last_seen ? iso(st.last_seen) : null, stay_open: st.stay_id !== null }); }
} else if (msgType === 'ACTIVITY_EVENT') {
  if (!st.online) { st.online = true; var p0 = tx(); p0.offline_min = st.last_seen ? Math.round((now - st.last_seen) / 60000) : null; p0.baby_state = st.baby_state; emit('incubator_online', p0); }
} else if (msgType === 'POST_ATTRIBUTES_REQUEST') {
  if (msg.baby_seq !== undefined) {
    var newSeq = parseInt(msg.baby_seq) || 0;
    if (newSeq !== st.baby_seq) st.baby_sex = null;   // otro ocupante: su sexo llega con él o no se sabe
    st.baby_seq = newSeq;
    if (msg.baby_admission_epoch !== undefined) st.admission_epoch = parseInt(msg.baby_admission_epoch) || 0;
    if (msg.baby_kangaroo_count !== undefined) st.kangaroo_count = parseInt(msg.baby_kangaroo_count) || 0;
    if (msg.baby_thermo_min !== undefined) st.thermo_min = parseInt(msg.baby_thermo_min) || 0;
    if (msg.baby_phototherapy_min !== undefined) st.photo_min = parseInt(msg.baby_phototherapy_min) || 0;
  }
  if (msg.baby_sex !== undefined) st.baby_sex = parseSex(msg.baby_sex);
} else if (msgType === 'POST_TELEMETRY_REQUEST') {
  st.last_seen = now;
  if (!st.online) { st.online = true; var p1 = tx(); p1.offline_min = null; p1.baby_state = st.baby_state; emit('incubator_online', p1); }
  // La IncuNest manda algunas claves solo al cambiar: sin ellas en el mensaje vale el último valor
  // guardado (metadata, latestTsKeyNames del nodo "estado previo + SN").
  var ca = lastOf('Control_active'), pa = lastOf('Phototherapy_active');
  var cm = lastOf('Control_mode'), td = lastOf('Temp_desired');
  if (ca !== undefined) st.control_active = truthy(ca);
  if (pa !== undefined) st.photo = truthy(pa);
  if (cm !== undefined) st.control_mode = String(cm);
  if (td !== undefined) st.temp_desired = num(td);
  // pulsioximetría
  var best = null, bestQ = 0;
  var hrs = [['HR1','HR1_SQI'],['HR2','HR2_SQI'],['HR3','HR3_SQI']];
  for (var i = 0; i < hrs.length; i++) { var q = num(msg[hrs[i][1]]); if (q !== null && q > bestQ) { bestQ = q; best = num(msg[hrs[i][0]]); } }
  var spo2q = num(msg.SpO2_SQI);
  st.spo2 = (spo2q !== null && spo2q > 0) || bestQ > 0;
  if (best !== null && bestQ >= 0.5) { st.hr = Math.round(best / 5) * 5; st.hr_q = 'good'; }
  else if (best !== null && bestQ > 0) { st.hr = null; st.hr_q = 'poor'; }
  else { st.hr = null; st.hr_q = 'none'; }
  // calor: solo del control. Las alarmas de la IncuNest no cuentan: IncuTwin nunca enseña alarmas
  // (tono sin sustos) y una alarma colgada dejaba "calor" con el control apagado (353, 25-nov).
  if (!st.control_active) { st.heat = 'off'; st.stable_since = null; }
  else {
    var meas = (st.control_mode === 'SKIN') ? num(msg.Skin_temp) : num(msg.Air_temp);
    if (st.temp_desired !== null && meas !== null && Math.abs(meas - st.temp_desired) <= 0.5) {
      if (!st.stable_since) st.stable_since = now;
      st.heat = (now - st.stable_since >= cfg.stable_ms) ? 'stable' : 'heating';
    } else { st.stable_since = null; st.heat = 'heating'; }
  }
  // eventos de bebé que llegan como telemetría (con ts propio)
  var evSeq = msg.baby_seq !== undefined ? parseInt(msg.baby_seq) : null;
  if (truthy(msg.baby_kangaroo_event) && evSeq === st.stay_seq && st.baby_state === 'in') {
    st.baby_state = 'parents'; st.parents_since = now; st.idle_since = null;
    if (msg.baby_kangaroo_count !== undefined) st.kangaroo_count = parseInt(msg.baby_kangaroo_count) || st.kangaroo_count;
    emit('baby_parents', { kangaroo_count: st.kangaroo_count });
  }
  if (msg.baby_outcome !== undefined && msg.baby_discharge_epoch !== undefined && evSeq === st.stay_seq && (st.baby_state === 'in' || st.baby_state === 'parents')) {
    if (msg.baby_thermo_min !== undefined) st.thermo_min = parseInt(msg.baby_thermo_min) || st.thermo_min;
    if (msg.baby_phototherapy_min !== undefined) st.photo_min = parseInt(msg.baby_phototherapy_min) || st.photo_min;
    if (msg.baby_kangaroo_count !== undefined) st.kangaroo_count = parseInt(msg.baby_kangaroo_count) || st.kangaroo_count;
    closeStay('discharged', parseInt(msg.baby_outcome), now);
  }
}

// ---- máquina de estados (para todo tipo de mensaje) ----
var care = st.control_active || st.photo;
st.care = care;
if (st.baby_state === 'none' || st.baby_state === 'out') {
  if (care) openStay(st.baby_seq > 0 ? st.baby_seq : 0);
} else {
  // estancia anónima y ahora registran al bebé: es el mismo, se adopta el seq (mismo stay_id)
  if (care && st.baby_seq > 0 && st.stay_seq === 0) { st.stay_seq = st.baby_seq; }
  else if (care && st.baby_seq > 0 && st.baby_seq !== st.stay_seq) { closeStay('replaced', null); openStay(st.baby_seq); }
  else if (st.baby_state === 'in') {
    if (!care) { if (!st.idle_since) st.idle_since = now; else if (now - st.idle_since >= cfg.t_idle_ms) closeStay('session_ended', null); }
    else st.idle_since = null;
  } else if (st.baby_state === 'parents') {
    if (care) { var away = Math.round((now - (st.parents_since || now)) / 60000); st.baby_state = 'in'; st.parents_since = null; var pb = idPayload(tx()); pb.away_min = away; emit('baby_back', pb); st.last_tx = JSON.stringify(tx()); st.last_tx_change = now; }
    else if (st.parents_since && now - st.parents_since >= cfg.t_parents_ms) closeStay('not_returned', null);
  }
}
// identidad: se fija al abrir el episodio (bebé nuevo = stay_id nuevo); si después cambia, la
// app y los paneles se enteran con baby_identity
if (st.baby_state === 'in' || st.baby_state === 'parents') {
  if (syncIdentity()) emit('baby_identity', idPayload({ reason: 'device_sex' }));
}
// treatment_changed (solo con bebé dentro, debounce 30 s)
if (st.baby_state === 'in' && msgType === 'POST_TELEMETRY_REQUEST') {
  var cur = JSON.stringify(tx());
  if (st.last_tx !== null && cur !== st.last_tx && now - st.last_tx_change >= cfg.tx_debounce_ms) {
    var prev = JSON.parse(st.last_tx), changed = [];
    for (var f in prev) if (prev[f] !== tx()[f]) changed.push(f);
    var pt = tx(); pt.changed = changed; emit('treatment_changed', pt);
    st.last_tx = cur; st.last_tx_change = now;
  } else if (st.last_tx === null) { st.last_tx = cur; st.last_tx_change = now; }
}
// heartbeat
if ((st.baby_state === 'in' || st.baby_state === 'parents') && st.online && now - st.last_hb >= cfg.hb_ms) {
  var ph = idPayload(tx()); ph.hr = st.hr; ph.hr_quality = st.hr_q; ph.baby_state = st.baby_state; emit('heartbeat', ph); st.last_hb = now;
}

// ---- salidas ----
// metadata.itw_out decide la rama en "switch salida"; msgType es el que exige cada nodo destino
function mdWith(out) { var m = {}; for (var mk in metadata) m[mk] = metadata[mk]; m.itw_out = out; return m; }
var out = [];
var inStay = (st.baby_state === 'in' || st.baby_state === 'parents');
var went_home = st.baby_state === 'out' && st.out_home === true;
// Todo bebé visible (dentro, con sus papás o recién ido a casa) lleva sexo. El nombre solo si
// es el real con consentimiento: el apodo no va al panel, que sin nombre pregunta al usuario
// (spec nombre-bebe-panel); se queda en itw_baby_alias y en los eventos del webhook.
var shown = (inStay || went_home) && !!st.id;
var name = '', sex = '', weight_g = 0, age_d = -1;
if (shown) {
  sex = st.id.sex;
  // Datos reales solo con consentimiento y solo si los client attributes siguen siendo de este
  // bebé (tras el alta la IncuNest los vacía o pasa al siguiente perfil).
  // (y nunca en una estancia anonima: sus cs_baby_* pueden ser de un bebe anterior)
  var babySeq = inStay ? st.stay_seq : st.out_seq;
  var sameBaby = babySeq > 0 && st.baby_seq === babySeq;
  if (metadata.ss_itw_show_name === 'true' && sameBaby) {
    name = clipUtf8(String(metadata.cs_baby_name || '').trim().substring(0, 20), 23);
    weight_g = parseInt(metadata.cs_baby_weight_g) || 0;
    // sin fecha de nacimiento en TB: "edad" = días desde el ingreso
    if (st.admission_epoch > 0) age_d = Math.max(0, Math.floor((now - st.admission_epoch * 1000) / 86400000));
  }
}
out.push({ msg: { itw_state: JSON.stringify(st), itw_baby_state: st.baby_state, itw_heat: st.heat, itw_photo: st.photo,
  itw_spo2: st.spo2, itw_hr: st.hr === null ? 0 : st.hr, itw_online: st.online,
  itw_stay_id: st.stay_id === null ? '' : st.stay_id, itw_seq: st.seq,  // TB rechaza atributos null
  itw_baby_sex: sex, itw_baby_sex_src: shown ? st.id.src : '', itw_baby_alias: shown ? st.id.alias : '' },
  metadata: mdWith('ITW_STATE'), msgType: 'POST_ATTRIBUTES_REQUEST' });
out.push({ msg: { online: st.online, thermo: st.heat, photo: st.photo, hr: st.hr === null ? 0 : st.hr,
  baby: st.baby_state, home: went_home, name: name, sex: sex, weight_g: weight_g, age_d: age_d, updated: now },
  metadata: mdWith('ITW_PANEL'), msgType: 'POST_ATTRIBUTES_REQUEST' });
for (var e = 0; e < events.length; e++) {
  var md = mdWith('ITW_EVENT');
  md.itw_event_type = events[e].event; md.ts = String(new Date(events[e].ts).getTime());
  out.push({ msg: events[e], metadata: md, msgType: 'ITW_EVENT' });
}
// ---- broker MQTT (docs/BROKER.md §3): estado completo, retenido, una vez por evento ----
// Solo cuando hay evento (baby_in/out, treatment_changed, heartbeat <= 1/60 s, offline/online):
// asi 353 incubadoras a 5 s no inundan el broker. event_seq = st.seq (monotono) para la dedup
// del panel. Ademas de los campos del contrato van las extensiones que el firmware acepta
// (thermo, baby, name, sex, weight_g, age_d) para canguro, alta a casa, alarma e identidad del bebé.
if (events.length > 0) {
  var inBaby = inStay;
  var tr = [];
  if (st.heat !== 'off') tr.push('heat');
  if (st.photo) tr.push('phototherapy');
  if (st.spo2) tr.push('pulseox');
  var babyExt = st.baby_state === 'in' ? 'in' : st.baby_state === 'parents' ? 'parents' : (went_home ? 'home' : 'none');
  var bp = { incubator_id: incubator, ts: Math.floor(now / 1000),
    state: !st.online ? 'offline' : (inBaby ? 'baby' : 'free'),
    treatments: tr, bpm: st.hr === null ? null : st.hr,
    last_seen: Math.floor((st.last_seen || now) / 1000), event_seq: st.seq,
    last_event: events[events.length - 1].event,
    thermo: st.heat, baby: babyExt };
  if (name) bp.name = name;
  if (st.stay_id) bp.stay_id = st.stay_id; // el panel guarda su propio nombre por estancia
  if (sex) bp.sex = sex;
  if (weight_g > 0) bp.weight_g = weight_g;
  if (age_d >= 0) bp.age_d = age_d;
  var cc = countryOf(); // spec tb-stay-history
  if (cc) bp.country = cc;
  var mb = mdWith('ITW_BROKER');
  mb.itw_incubator_id = incubator; // topic incubators/${itw_incubator_id}/state en el nodo MQTT
  out.push({ msg: bp, metadata: mb, msgType: 'POST_TELEMETRY_REQUEST' });
}
return out;
