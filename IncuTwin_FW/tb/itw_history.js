// ThingsBoard rule node: Transformation script (JS, ES5). El sandbox Nashorn de ThingsBoard exige
// llaves en todo for/while (BracesException): nada de bucles de una linea sin { }.
// Historial de estancias por panel — spec tb-stay-history (openspec/changes/tb-stay-history)
// Originator: el panel IncuTwin (device del IoT Gateway, nombre = client id).
// metadata.ss_itw_history        -> server attribute del panel (JSON; ausente o roto = vacio)
// metadata.ss_itw_utc_offset_min -> dia local del panel (60 por defecto)
// metadata.ts                    -> ahora (ms)
// msg (lo arma la cadena en el lado de la IncuNest, antes de cambiar el originador):
//   kind        'stay_open' | 'stay_close' | 'hold' | 'sync' | 'unpaired'
//   stay_id     "SN:baby_seq" (ausente en sync sin estancia en curso y en unpaired)
//   in_since    ms de entrada de la estancia (de itw_state de la IncuNest)
//   ts          ms del evento (stay_close: fecha del alta)
//   outcome     'home' o cualquier otra cosa (stay_close)
//   duration_days               (stay_close)
//   country     ISO alfa-2 de la IncuNest ("" si no hay)
//   show_name   itw_show_name de la IncuNest; name = baby_name
//   panel_count itw_panel_count de la IncuNest
//   panel       nombre del device del panel en ThingsBoard (ITW-DEEF3C o incutwin-DEEF3C)
// Salida: SAVE (itw_history -> server attribute del panel) y PUBLISH (mensaje al panel, topic
// incutwin/${itw_client}/cmd/history, retenido; itw_client = client id del broker, como
// tools/broker_mqtt.client_id_for: ITW-DEEF3C -> incutwin-DEEF3C). El mensaje nunca lleva hold_days, ld,
// desenlaces distintos de "home" ni datos clinicos.

var DAY = 86400000;
var MAX_STAYS = 10;
var MAX_HOLD_DAYS = 60;
var MAX_BYTES = 1000;

var now = metadata.ts ? parseInt(metadata.ts) : new Date().getTime();
var tz = parseInt(metadata.ss_itw_utc_offset_min);
if (isNaN(tz) || tz < -720 || tz > 840) tz = 60;

function dayNum(ms) { return Math.floor((ms + tz * 60000) / DAY); }
function dayStr(n) { return new Date(n * DAY).toISOString().substring(0, 10); }
function truthy(v) { return v === true || v === 'true' || v === 1 || v === '1'; }
function utf8Len(s) { return encodeURIComponent(s).replace(/%[0-9A-F]{2}/g, 'x').length; }
function clientIdFor(n) {
  n = String(n || '').replace(/^\s+|\s+$/g, '');
  if (n.indexOf('incutwin-') === 0) return n;
  if (n.toUpperCase().indexOf('ITW-') === 0) n = n.substring(4);
  return 'incutwin-' + n;
}
var panelName = String(msg.panel || metadata.deviceName || '');
function mdWith(out) {
  var m = {}; for (var k in metadata) { m[k] = metadata[k]; }
  m.itw_out = out; m.itw_panel = panelName; m.itw_client = clientIdFor(panelName);
  return m;
}

var h = null;
try { if (metadata.ss_itw_history) h = JSON.parse(metadata.ss_itw_history); } catch (e) { h = null; }
if (!h || h.v !== 1 || !(h.stays instanceof Array)) h = { v: 1, stays: [], hold_days: [], days_total: 0 };
if (!(h.hold_days instanceof Array)) h.hold_days = [];
if (typeof h.days_total !== 'number') h.days_total = h.hold_days.length;

var kind = String(msg.kind || '');
var stayId = msg.stay_id ? String(msg.stay_id) : '';
var inSince = parseInt(msg.in_since) || 0;
var country = (typeof msg.country === 'string' && /^[A-Z]{2}$/.test(msg.country)) ? msg.country : '';
var name = truthy(msg.show_name) ? String(msg.name || '').trim().substring(0, 20) : '';
var today = dayNum(now);

function find(id) {
  for (var i = 0; i < h.stays.length; i++) { if (h.stays[i].id === id) { return h.stays[i]; } }
  return null;
}

function create(id, fromMs) {
  var s = { id: id, c: country, n: name, from: dayStr(dayNum(fromMs || now)), to: null, end: null,
            days: 0, holds: 0, my_days: 0, ld: null, since: fromMs || now };
  h.stays.unshift(s);
  while (h.stays.length > MAX_STAYS) { h.stays.pop(); }
  return s;
}

// estancia en curso: datos que manda la IncuNest
function refresh(s) {
  s.c = country || s.c;
  s.n = name;
  if (inSince && !s.since) s.since = inSince; // una reapertura no cambia la fecha de entrada
  s.days = Math.max(0, Math.floor((now - (s.since || now)) / DAY));
}

// la IncuNest dice que la estancia sigue: abrirla (alta por error, re-emparejado) o crearla
function ensureOpen(id) {
  var s = find(id);
  if (!s) s = create(id, inSince || now);
  else if (s.to !== null) { s.to = null; s.end = null; }
  refresh(s);
  return s;
}

if (kind === 'stay_open' && stayId) {
  ensureOpen(stayId);
} else if (kind === 'stay_close' && stayId) {
  var sc = find(stayId);
  var when = parseInt(msg.ts) || now;
  var dur = parseFloat(msg.duration_days);
  if (!sc) sc = create(stayId, isNaN(dur) ? when : when - dur * DAY);
  if (sc.to === null) { // ya cerrada (o acompanada hasta un desemparejamiento): no se toca
    var home = msg.outcome === 'home';
    sc.to = dayStr(dayNum(when));
    sc.end = home ? 'home' : 'left';
    sc.c = country || sc.c;
    sc.n = home ? (name || sc.n) : '';
    sc.days = isNaN(dur) ? Math.max(0, Math.floor((when - (sc.since || when)) / DAY)) : Math.round(dur);
  }
} else if (kind === 'hold' && stayId) {
  var sh = ensureOpen(stayId);
  sh.holds++;
  if (sh.ld !== today) { sh.my_days++; sh.ld = today; }
  if (h.hold_days.indexOf(today) < 0) {
    h.hold_days.push(today);
    h.hold_days.sort(function (a, b) { return a - b; });
    while (h.hold_days.length > MAX_HOLD_DAYS) { h.hold_days.shift(); }
    h.days_total++;
  }
} else if (kind === 'sync') {
  if (stayId) ensureOpen(stayId);
} else if (kind === 'unpaired') {
  for (var u = 0; u < h.stays.length; u++) {
    if (h.stays[u].to === null) h.stays[u].to = dayStr(today); // acompanada hasta hoy; end sigue null
  }
}

// ---- logros ----
var hasOpen = false;
for (var o = 0; o < h.stays.length; o++) { if (h.stays[o].to === null) { hasOpen = true; } }
var pc = parseInt(msg.panel_count);
var families = (hasOpen && !isNaN(pc)) ? Math.max(0, pc - 1) : 0;

var last = h.hold_days.length ? h.hold_days[h.hold_days.length - 1] : null;
var streak = 0;
if (last !== null && today - last <= 1) {
  streak = 1;
  for (var d = h.hold_days.length - 2; d >= 0 && h.hold_days[d] === h.hold_days[d + 1] - 1; d--) { streak++; }
}
h.upd = Math.floor(now / 1000);

// ---- salidas ----
var stays = [];
for (var p = 0; p < h.stays.length; p++) {
  var s0 = h.stays[p];
  stays.push({ id: s0.id, c: s0.c, n: s0.n, from: s0.from, to: s0.to, end: s0.end,
               days: s0.days, holds: s0.holds, my_days: s0.my_days });
}
var pub = { v: 1, upd: h.upd, tz: tz,
            stats: { days: h.days_total, streak: streak, last: last === null ? null : dayStr(last), families: families },
            stays: stays };
while (stays.length > 1 && utf8Len(JSON.stringify(pub)) > MAX_BYTES) { stays.pop(); }

return [
  { msg: { itw_history: JSON.stringify(h) }, metadata: mdWith('SAVE'), msgType: 'POST_ATTRIBUTES_REQUEST' },
  { msg: pub, metadata: mdWith('PUBLISH'), msgType: 'POST_TELEMETRY_REQUEST' }
];
