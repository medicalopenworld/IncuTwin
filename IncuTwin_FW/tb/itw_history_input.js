// ThingsBoard rule node: Transformation script (JS, ES5)
// Entrada del historial (spec tb-stay-history): traduce, en el lado de la IncuNest, lo que dispara
// el historial a la entrada de itw_history.js. tb_apply_history.py sustituye __MODE__:
//   'event' -> IncuNest_IncuTwin_state, salida ITW_EVENT (msg = sobre del evento v0.2).
//              Devuelve un ARRAY: una entrada por panel de itw_panels (nodo "split array msg").
//   'hold'  -> IncuTwin_hand_hold, salida LOG (msg.itw_hand_hold = JSON con panel y source).
//   'sync'  -> cadena IncuTwin del panel, tras pasar a la IncuNest (msg = {kind, panel}).
// metadata (atributos de la IncuNest): ss_itw_state, ss_country (o telemetría countryCode), ss_itw_show_name,
//   ss_itw_panels (JSON array de nombres de panel), ss_itw_panel_count, cs_baby_name.
//   En 'event', ss_itw_state es el estado ANTERIOR (se leyo antes de la maquina de estados).
// Cada entrada lleva `panel` (nombre del device del panel): la cadena cambia el originador con
// el patron $[panel].

var MODE = '__MODE__';

var st = {};
try { st = JSON.parse(metadata.ss_itw_state || '{}') || {}; } catch (e) { st = {}; }
var panels = [];
try { panels = JSON.parse(metadata.ss_itw_panels || '[]'); } catch (e) { panels = []; }
if (!(panels instanceof Array)) panels = [];
var pc = parseInt(metadata.ss_itw_panel_count);
if (isNaN(pc)) pc = panels.length;
function isoCountry(v) {
  var c = String(v || '').toUpperCase();
  return (/^[A-Z]{2}$/.test(c) && c !== 'XX') ? c : '';
}
// país: el atributo de MOW manda; si no, el countryCode que manda la IncuNest por telemetría
var cc = isoCountry(metadata.ss_country) || isoCountry(metadata.countryCode);
var showName = metadata.ss_itw_show_name === 'true';
var now = metadata.ts ? parseInt(metadata.ts) : new Date().getTime();

function base(kind, panel) {
  return { kind: kind, panel: String(panel || ''), country: cc,
           show_name: showName, name: showName ? String(metadata.cs_baby_name || '') : '',
           panel_count: pc };
}
function inCare() { return st.baby_state === 'in' || st.baby_state === 'parents'; }

var out;
if (MODE === 'event') {
  var ev = String(msg.event || '');
  var ts = Date.parse(msg.ts) || now;
  var p = msg.payload || {};
  out = [];
  for (var i = 0; i < panels.length; i++) {
    var m = base(ev === 'baby_out' ? 'stay_close' : 'stay_open', panels[i]);
    m.stay_id = String(msg.stay_id || '');
    m.ts = ts;
    if (ev === 'baby_out') {
      m.outcome = p.outcome === 'home' ? 'home' : null;
      if (p.duration_days !== undefined && p.duration_days !== null) m.duration_days = p.duration_days;
    } else {
      m.in_since = ev === 'baby_in' ? ts : (st.in_since || ts);
    }
    out.push(m);
  }
} else if (MODE === 'hold') {
  var j = {};
  try { j = JSON.parse(msg.itw_hand_hold || '{}') || {}; } catch (e) { j = {}; }
  out = base('hold', j.panel);
  out.stay_id = st.stay_id ? String(st.stay_id) : '';
  out.in_since = st.in_since || now;
  metadata.itw_panel = out.panel;
} else { // sync
  out = base('sync', msg.panel);
  if (inCare() && st.stay_id) { out.stay_id = String(st.stay_id); out.in_since = st.in_since || now; }
  metadata.itw_panel = out.panel;
}
return { msg: out, metadata: metadata, msgType: 'ITW_HISTORY' };
