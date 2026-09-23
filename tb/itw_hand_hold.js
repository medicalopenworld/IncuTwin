// ThingsBoard rule node: Transformation script (JS)
// "Coge mi mano" — Contrato de eventos IncuTwin v0.2 §3.5
// Originator: la IncuNest (ya resuelta por la cadena que llama).
// msg: { hand_hold_minutes?, source: 'panel'|'app', panel? }
// metadata.ss_incutwin_enabled, ss_itw_baby_state -> atributos de servidor de la IncuNest
// Salida (metadata.itw_out decide la rama):
//   HH     -> shared attr hand_hold_until (IncuNest y sus paneles)
//   LOG    -> telemetría itw_hand_hold (auditoría)
//   REJECT -> telemetría itw_hand_hold_rejected

var now = new Date().getTime();
function mdWith(out) { var m = {}; for (var k in metadata) m[k] = metadata[k]; m.itw_out = out; return m; }

var source = msg.source ? String(msg.source) : 'unknown';
var reason = null;
if (metadata.ss_incutwin_enabled !== 'true') reason = 'not_enabled';
else if (metadata.ss_itw_baby_state !== 'in' && metadata.ss_itw_baby_state !== 'parents') reason = 'no_baby';

if (reason) {
  return { msg: { itw_hand_hold_rejected: JSON.stringify({ reason: reason, source: source, at: now }) },
           metadata: mdWith('REJECT'), msgType: 'POST_TELEMETRY_REQUEST' };
}

var minutes = parseInt(msg.hand_hold_minutes) || 5;
if (minutes < 1) minutes = 1;
if (minutes > 30) minutes = 30;
var until = now + minutes * 60000;

return [
  { msg: { hand_hold_until: until }, metadata: mdWith('HH'), msgType: 'POST_ATTRIBUTES_REQUEST' },
  { msg: { itw_hand_hold: JSON.stringify({ source: source, minutes: minutes, until: until,
           baby_state: metadata.ss_itw_baby_state, panel: msg.panel || null }) },
    metadata: mdWith('LOG'), msgType: 'POST_TELEMETRY_REQUEST' }
];
