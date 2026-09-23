#!/usr/bin/env python3
"""Visor de demo IncuTwin: hace de "app del padrino" leyendo los itw_event de una IncuNest.

Uso:
  TB_URL=https://mon.medicalopenworld.org TB_API_KEY=... python tools/demo_viewer.py [--device IncuNest-1_2] [--sn 1] [--port 8765]
  y abrir http://localhost:8765

- Lee por REST la telemetría itw_event / itw_hand_hold de la incubadora (rama "estado del
  gemelo" del Contrato v0.2) y la pinta como línea de tiempo en lenguaje de padrino.
- El botón "Coge mi mano" publica en el device IncuTwin_cloud_bridge, igual que hará la app.
- Credenciales SOLO en el servidor (variables de entorno); el navegador no ve ni la API key
  ni el token del bridge. Para algo más que una demo local: usar una API key de un usuario de
  solo lectura y ITW_BRIDGE_TOKEN en lugar de leer el token con la key de administrador.
"""
import argparse, json, os, time, requests
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ap = argparse.ArgumentParser()
ap.add_argument("--device", default="IncuNest-1_2")
ap.add_argument("--sn", default="1", help="incubator_id que manda la app (telemetría SN)")
ap.add_argument("--port", type=int, default=8765)
ap.add_argument("--hours", type=float, default=12, help="ventana de eventos a mostrar")
args = ap.parse_args()

TB = os.environ.get("TB_URL", "https://mon.medicalopenworld.org").rstrip("/")
s = requests.Session()
if os.environ.get("TB_API_KEY"):
    s.headers["X-Authorization"] = "ApiKey " + os.environ["TB_API_KEY"]
else:
    tok = s.post(f"{TB}/api/auth/login", json={"username": os.environ["TB_USER"], "password": os.environ["TB_PASS"]}).json()["token"]
    s.headers["X-Authorization"] = "Bearer " + tok

def get(u):
    r = s.get(TB + u, timeout=10); r.raise_for_status(); return r.json()

DEV = get(f"/api/tenant/devices?deviceName={args.device}")["id"]["id"]
BRIDGE_TOKEN = os.environ.get("ITW_BRIDGE_TOKEN")
if not BRIDGE_TOKEN:
    bridge = get("/api/tenant/devices?deviceName=IncuTwin_cloud_bridge")["id"]["id"]
    BRIDGE_TOKEN = get(f"/api/device/{bridge}/credentials")["credentialsId"]
print(f"visor: {args.device} ({DEV}) -> http://localhost:{args.port}")

def events():
    now = int(time.time() * 1000)
    q = (f"/api/plugins/telemetry/DEVICE/{DEV}/values/timeseries?keys=itw_event,itw_hand_hold"
         f"&startTs={now - int(args.hours * 3600000)}&endTs={now}&limit=300&orderBy=DESC")
    ts = get(q)
    out = []
    for p in ts.get("itw_event", []):
        try: e = json.loads(p["value"])
        except Exception: continue
        if e.get("event") == "heartbeat": continue
        out.append({"ts": p["ts"], "event": e["event"], "payload": e.get("payload", {}), "stay": e.get("stay_id")})
    for p in ts.get("itw_hand_hold", []):
        try: h = json.loads(p["value"])
        except Exception: continue
        out.append({"ts": p["ts"], "event": "hand_hold", "payload": h})
    out.sort(key=lambda e: e["ts"], reverse=True)
    attrs = {a["key"]: a["value"] for a in get(
        f"/api/plugins/telemetry/DEVICE/{DEV}/values/attributes/SERVER_SCOPE?keys=itw_baby_state,itw_heat,itw_photo,itw_spo2,itw_hr,itw_online")}
    shared = {a["key"]: a["value"] for a in get(
        f"/api/plugins/telemetry/DEVICE/{DEV}/values/attributes/SHARED_SCOPE?keys=hand_hold_until")}
    return {"now": now, "state": attrs, "hand_hold_until": shared.get("hand_hold_until", 0), "events": out[:80]}

def hand_hold():
    r = requests.post(f"{TB}/api/v1/{BRIDGE_TOKEN}/telemetry",
                      json={"incubator_id": args.sn, "hand_hold_minutes": 5}, timeout=10)
    time.sleep(1.5)  # la rule chain escribe el resultado; se lee para responder
    now = int(time.time() * 1000)
    ts = get(f"/api/plugins/telemetry/DEVICE/{DEV}/values/timeseries?keys=itw_hand_hold,itw_hand_hold_rejected"
             f"&startTs={now - 10000}&endTs={now}&limit=1")
    if ts.get("itw_hand_hold"): return {"ok": True}
    rej = ts.get("itw_hand_hold_rejected")
    reason = json.loads(rej[0]["value"]).get("reason") if rej else ("http_%d" % r.status_code)
    return {"ok": False, "reason": reason}

PAGE = r"""<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IncuTwin demo</title>
<style>
:root{--bg:#fbf6f1;--card:#fff;--ink:#3a2f2a;--mute:#8a7d75;--coral:#f06f5a;--warm:#f2a03d;--ok:#4fae84;--photo:#4a90d9;--line:#eadfd6}
@media (prefers-color-scheme:dark){:root{--bg:#1d1916;--card:#29231f;--ink:#f3ebe4;--mute:#a59890;--line:#3a322c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:460px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:15px;letter-spacing:.08em;text-transform:uppercase;color:var(--mute);margin:0 0 14px}
.card{background:var(--card);border-radius:20px;padding:20px;box-shadow:0 2px 10px rgba(0,0,0,.06);margin-bottom:16px}
#now{font-size:22px;font-weight:650;margin:0 0 6px}#sub{color:var(--mute);margin:0}
.chips{display:flex;gap:8px;margin-top:14px;flex-wrap:wrap}
.chip{border-radius:999px;padding:5px 12px;font-size:14px;background:var(--line);color:var(--mute)}
.chip.on.heat{background:var(--warm);color:#fff}.chip.on.stable{background:var(--ok);color:#fff}
.chip.on.photo{background:var(--photo);color:#fff}.chip.on.heart{background:var(--coral);color:#fff}
button{width:100%;border:0;border-radius:16px;padding:16px;font-size:18px;font-weight:650;background:var(--coral);color:#fff;cursor:pointer}
button:disabled{opacity:.5;cursor:default}#hmsg{min-height:1.4em;color:var(--mute);text-align:center;margin:10px 0 0;font-size:14px}
ol{list-style:none;margin:0;padding:0}li{display:flex;gap:12px;padding:12px 0;border-bottom:1px solid var(--line)}
li:last-child{border:0}.t{color:var(--mute);font-size:13px;min-width:44px;padding-top:2px}
.dot{width:10px;height:10px;border-radius:50%;margin-top:6px;flex:none;background:var(--line)}
.dot.in{background:var(--ok)}.dot.parents{background:var(--coral)}.dot.home{background:var(--ok);box-shadow:0 0 0 4px rgba(79,174,132,.25)}
.dot.hand{background:var(--coral)}.dot.tx{background:var(--warm)}
.home-banner{font-size:26px;text-align:center}
</style></head><body><main>
<h1>IncuTwin · demo</h1>
<section class="card"><p id="now">Conectando…</p><p id="sub"></p>
<div class="chips"><span class="chip" id="c-heat">Calor</span><span class="chip" id="c-photo">Luz</span><span class="chip" id="c-heart">Corazón</span></div></section>
<section class="card"><button id="hand">Coge mi mano</button><p id="hmsg"></p></section>
<section class="card"><ol id="tl"></ol></section>
</main><script>
const $=id=>document.getElementById(id);
const hm=ts=>new Date(ts).toLocaleTimeString('es-ES',{hour:'2-digit',minute:'2-digit'});
function txText(p){const a=[];
 if(p.heat==='heating')a.push('la incubadora le está dando calor');else if(p.heat==='stable')a.push('ya está calentito');else if(p.heat==='alarm')a.push('las enfermeras están ajustando el calor');
 if(p.photo)a.push('le dan luz azul para que se ponga fuerte');
 if(p.spo2)a.push('le vigilan el corazón');
 return a.length?a.join(', '):'descansa sin tratamientos';}
function text(e){const p=e.payload||{};switch(e.event){
 case'baby_in':return['in','Tu bebé ha llegado a la incubadora: '+txText(p)];
 case'treatment_changed':return['tx','Novedad: '+txText(p)];
 case'baby_parents':return['parents','Está con sus papás, piel con piel'];
 case'baby_back':return['in','Ha vuelto a la incubadora'+(p.away_min?` tras ${p.away_min} min con sus papás`:'')];
 case'baby_out':return p.outcome==='home'?['home',`¡Se ha ido a casa! 🎉 ${p.duration_days!=null?'Tras '+p.duration_days+' días de cuidados':''}`]:['',"Su estancia en la incubadora ha terminado"];
 case'incubator_offline':return['','La incubadora se ha quedado sin conexión un momento'];
 case'incubator_online':return['','La incubadora vuelve a estar conectada'];
 case'hand_hold':return['hand',p.source==='panel'?'Alguien le ha cogido la mano desde su IncuTwin':'Alguien le ha cogido la mano desde la app'];
 default:return['',e.event];}}
function nowText(st,home){const b=st.itw_baby_state;
 if(st.itw_online===false)return['La incubadora no tiene conexión','Te avisaremos cuando vuelva'];
 if(b==='in')return['Tu bebé está en la incubadora',txText({heat:st.itw_heat,photo:st.itw_photo,spo2:st.itw_spo2})];
 if(b==='parents')return['Está con sus papás','Piel con piel, el mejor calor'];
 if(b==='out'&&home)return['¡Se ha ido a casa! 🎉','Gracias por acompañarle'];
 return['La incubadora está esperando','Todavía no hay ningún bebé'];}
async function poll(){try{const r=await fetch('/api/events');const d=await r.json();const st=d.state;
 const lastOut=d.events.find(e=>e.event==='baby_out'||e.event==='baby_in');
 const home=lastOut&&lastOut.event==='baby_out'&&lastOut.payload.outcome==='home';
 const [a,b]=nowText(st,home);$('now').textContent=a;$('sub').textContent=b;
 const inside=st.itw_baby_state==='in';
 $('c-heat').className='chip '+(inside&&st.itw_heat==='stable'?'on stable':inside&&st.itw_heat==='heating'?'on heat':'');
 $('c-photo').className='chip '+(inside&&st.itw_photo?'on photo':'');
 $('c-heart').className='chip '+(inside&&st.itw_spo2?'on heart':'');
 const hh=d.hand_hold_until>d.now;$('hand').textContent=hh?'Le estás cogiendo la mano 🤍':'Coge mi mano';
 $('tl').innerHTML=d.events.map(e=>{const[c,t]=text(e);return`<li><span class="t">${hm(e.ts)}</span><span class="dot ${c}"></span><span>${t}</span></li>`}).join('')||'<li>Sin novedades todavía</li>';
}catch(e){$('now').textContent='Sin conexión con el visor';}}
$('hand').onclick=async()=>{$('hand').disabled=true;$('hmsg').textContent='Enviando…';
 try{const r=await(await fetch('/api/hand',{method:'POST'})).json();
  $('hmsg').textContent=r.ok?'La incubadora ya sabe que estás con la familia':(r.reason==='no_baby'?'Ahora no hay ningún bebé en la incubadora':'No se ha podido enviar ('+r.reason+')');}
 catch(e){$('hmsg').textContent='No se ha podido enviar';}
 $('hand').disabled=false;poll();};
poll();setInterval(poll,3000);
</script></body></html>"""

class H(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json"):
        b = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code); self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.path == "/": return self._send(200, PAGE, "text/html")
        if self.path == "/api/events":
            try: return self._send(200, json.dumps(events()))
            except Exception as e: return self._send(502, json.dumps({"error": str(e)}))
        self._send(404, "{}")
    def do_POST(self):
        if self.path == "/api/hand":
            try: return self._send(200, json.dumps(hand_hold()))
            except Exception as e: return self._send(502, json.dumps({"ok": False, "reason": str(e)}))
        self._send(404, "{}")
    def log_message(self, *a): pass

ThreadingHTTPServer(("127.0.0.1", args.port), H).serve_forever()
