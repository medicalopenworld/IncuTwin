// País al broker: el atributo de servidor `country` (lo pone MOW) manda; si no hay, el
// countryCode que la IncuNest manda por telemetría (geolocalización). "XX" = desconocido.
const fs = require('fs');
const assert = require('assert');
const fn = new Function('msg', 'metadata', 'msgType',
  fs.readFileSync(require('path').join(__dirname, 'itw_state_machine.js'), 'utf8'));

const T = Date.parse('2026-11-21T10:00:00Z');
function brokerCountry(md, msg) {
  const out = fn(msg || { Control_active: true, Phototherapy_active: true, Air_temp: 33 },
    Object.assign({ deviceName: '8944501606231786020', SN: '297', ts: String(T) }, md),
    'POST_TELEMETRY_REQUEST');
  const br = out.find(o => o.metadata.itw_out === 'ITW_BROKER');
  assert.ok(br, 'publica en el broker');
  return br.msg.country;
}

assert.strictEqual(brokerCountry({ countryCode: 'TG' }), 'TG', 'telemetría countryCode sin atributo');
assert.strictEqual(brokerCountry({ countryCode: 'tg' }), 'TG', 'en mayúsculas');
assert.strictEqual(brokerCountry({ ss_country: 'cm', countryCode: 'TG' }), 'CM', 'el atributo manda');
assert.strictEqual(brokerCountry({ countryCode: 'XX' }), undefined, 'XX es desconocido');
assert.strictEqual(brokerCountry({ ss_country: 'XX', countryCode: 'TG' }), 'TG', 'atributo XX no tapa la telemetría');
assert.strictEqual(brokerCountry({}, { Control_active: true, Phototherapy_active: false, countryCode: 'TG' }), 'TG',
  'countryCode en el propio mensaje');
assert.strictEqual(brokerCountry({ countryCode: 'Togo' }), undefined, 'solo ISO alfa-2');
console.log('ok test_sm_country');
