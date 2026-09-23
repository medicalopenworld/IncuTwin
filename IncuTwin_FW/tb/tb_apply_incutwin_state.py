#!/usr/bin/env python3
"""Despliega la rama "estado del gemelo" (Contrato IncuTwin v0.2 §3.4) en ThingsBoard.

Idempotente: se puede relanzar; actualiza la rule chain si ya existe.
Uso:
  TB_URL=https://mon.medicalopenworld.org TB_API_KEY=... python3 tb_apply_incutwin_state.py [--enable TEST-incubadora]
  (o TB_USER/TB_PASS en lugar de TB_API_KEY)
Hace:
  1. Crea/actualiza la rule chain "IncuNest_IncuTwin_state".
  2. Añade a la rule chain raíz un nodo "rule chain" hacia ella, conectado desde el
     Message Type Switch en: Post telemetry, Post attributes, Activity Event, Inactivity Event.
  3. Pone incutwin_enabled=true e inactivityTimeout=180000 en las incubadoras indicadas con --enable
     (más --attr=clave=valor, p. ej. --attr=itw_show_name=true para mostrar el nombre en el panel).
No toca ningún otro nodo ni dispositivo.
"""
import json, os, sys, pathlib, requests

TB   = os.environ.get("TB_URL", "https://mon.medicalopenworld.org").rstrip("/")
HERE = pathlib.Path(__file__).parent
CHAIN_NAME = "IncuNest_IncuTwin_state"
INPUT_NODE_NAME = "→ IncuTwin state"

s = requests.Session()
if os.environ.get("TB_API_KEY"):
    s.headers.update({"X-Authorization": f"ApiKey {os.environ['TB_API_KEY']}"})
else:
    tok = s.post(f"{TB}/api/auth/login", json={"username": os.environ["TB_USER"], "password": os.environ["TB_PASS"]}).json()["token"]
    s.headers.update({"X-Authorization": f"Bearer {tok}"})
s.headers.update({"Content-Type": "application/json"})
def get(u): r = s.get(f"{TB}{u}"); r.raise_for_status(); return r.json()
def post(u, b): r = s.post(f"{TB}{u}", json=b); r.raise_for_status(); return r.json() if r.text else None

def node(t, name, cfg, ver, x, y, desc=""):
    return {"type": f"org.thingsboard.rule.engine.{t}", "name": name, "configuration": cfg,
            "configurationVersion": ver, "additionalInfo": {"layoutX": x, "layoutY": y, "description": desc}, "debugSettings": None}

def chain_metadata(rc_id):
    sm = (HERE / "itw_state_machine.js").read_text(encoding="utf-8")
    ev2ts = (HERE / "itw_event_to_ts.js").read_text(encoding="utf-8")
    nodes = [
      node("metadata.TbGetAttributesNode", "¿incutwin_enabled?",
           {"tellFailureIfAbsent": False, "clientAttributeNames": [], "sharedAttributeNames": [],
            "serverAttributeNames": ["incutwin_enabled"], "latestTsKeyNames": [], "getLatestValueWithTs": False, "fetchTo": "METADATA"}, 1, 150, 200,
           "Solo lee un atributo (cacheado). Las 353 incubadoras pasan por aquí; solo las habilitadas siguen."),
      node("filter.TbJsFilterNode", "filtro incutwin_enabled",
           {"scriptLang": "JS", "jsScript": "return metadata.ss_incutwin_enabled === 'true';", "tbelScript": "return metadata.ss_incutwin_enabled == 'true';"}, 0, 400, 200),
      node("metadata.TbGetAttributesNode", "estado previo + SN",
           {"tellFailureIfAbsent": False,
            "clientAttributeNames": ["baby_seq", "baby_admission_epoch", "baby_kangaroo_count", "baby_thermo_min", "baby_phototherapy_min", "baby_name", "baby_weight_g"],
            "sharedAttributeNames": [],
            "serverAttributeNames": ["itw_state", "itw_t_parents_min", "itw_t_idle_min", "itw_show_name"], "latestTsKeyNames": ["SN"], "getLatestValueWithTs": False, "fetchTo": "METADATA"}, 1, 650, 200),
      node("transform.TbTransformMsgNode", "máquina de estados IncuTwin",
           {"scriptLang": "JS", "jsScript": sm, "tbelScript": "return msg;"}, 0, 900, 200,
           "Contrato v0.2 §3.4. Emite ITW_STATE (server attrs), ITW_PANEL (shared attrs a paneles) e ITW_EVENT (webhook)."),
      node("filter.TbJsSwitchNode", "switch salida",
           {"scriptLang": "JS", "jsScript": "return [metadata.itw_out];", "tbelScript": "return [metadata.itw_out];"}, 0, 1150, 200),
      node("telemetry.TbMsgAttributesNode", "guardar itw_state (server)",
           {"scope": "SERVER_SCOPE", "notifyDevice": False, "sendAttributesUpdatedNotification": False,
            "updateAttributesOnlyOnValueChange": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 3, 1450, 50),
      node("transform.TbChangeOriginatorNode", "→ paneles MirroredBy",
           {"originatorSource": "RELATED", "entityType": None, "entityNamePattern": None,
            "relationsQuery": {"direction": "FROM", "maxLevel": 1, "fetchLastLevelOnly": False,
                               "filters": [{"relationType": "MirroredBy", "entityTypes": ["DEVICE"]}]}}, 1, 1450, 200,
           "Sin panel emparejado → Failure (no conectado): normal."),
      node("telemetry.TbMsgAttributesNode", "shared attrs al panel",
           {"scope": "SHARED_SCOPE", "notifyDevice": True, "sendAttributesUpdatedNotification": False,
            "updateAttributesOnlyOnValueChange": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 3, 1750, 200),
      node("transform.TbTransformMsgNode", "evento → telemetría auditable",
           {"scriptLang": "JS", "jsScript": ev2ts, "tbelScript": "return msg;"}, 0, 1450, 350),
      node("telemetry.TbMsgTimeseriesNode", "guardar itw_event (TTL 30d)",
           {"defaultTTL": 2592000, "useServerTs": False, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 1, 1750, 350,
           "Registro de eventos emitidos, con el ts del evento. Sirve de fixture para NEXT."),
      node("rest.TbRestApiCallNode", "webhook Firebase (pendiente URL)",
           {"restEndpointUrlPattern": "${ss_itw_webhook_url}", "requestMethod": "POST", "useSimpleClientHttpFactory": False,
            "parseToPlainText": False, "ignoreRequestBody": False, "enableProxy": False, "readTimeoutMs": 5000, "maxParallelRequestsCount": 0,
            "headers": {"Content-Type": "application/json", "Authorization": "Bearer ${ss_itw_webhook_token}"},
            "credentials": {"type": "anonymous"}}, 0, 1750, 500,
           "NO CONECTADO. Cuando NEXT dé la URL: poner itw_webhook_url e itw_webhook_token como server attrs del TENANT (o de la incubadora) y conectar desde 'switch salida' con ITW_EVENT."),
    ]
    conns = [
      {"fromIndex": 0, "toIndex": 1, "type": "Success"},
      {"fromIndex": 1, "toIndex": 2, "type": "True"},
      {"fromIndex": 2, "toIndex": 3, "type": "Success"},
      {"fromIndex": 3, "toIndex": 4, "type": "Success"},
      {"fromIndex": 4, "toIndex": 5, "type": "ITW_STATE"},
      {"fromIndex": 4, "toIndex": 6, "type": "ITW_PANEL"},
      {"fromIndex": 6, "toIndex": 7, "type": "Success"},
      {"fromIndex": 4, "toIndex": 8, "type": "ITW_EVENT"},
      {"fromIndex": 8, "toIndex": 9, "type": "Success"},
    ]
    return {"ruleChainId": rc_id, "firstNodeIndex": 0, "nodes": nodes, "connections": conns, "ruleChainConnections": None}

# 1. rule chain
chains = get("/api/ruleChains?pageSize=200&page=0")["data"]
rc = next((c for c in chains if c["name"] == CHAIN_NAME), None)
if not rc:
    rc = post("/api/ruleChain", {"name": CHAIN_NAME, "type": "CORE", "root": False, "debugMode": False,
              "additionalInfo": {"description": "Rama 'estado del gemelo' del Contrato de eventos IncuTwin v0.2. Entrada desde la raíz de IncuNest (telemetría, atributos, actividad). Filtra por server attribute incutwin_enabled."}})
    print("creada rule chain", CHAIN_NAME)
post("/api/ruleChain/metadata", chain_metadata(rc["id"]))
print("metadata actualizada:", CHAIN_NAME, rc["id"]["id"])

# 2. enganche en la raíz
root = next(c for c in chains if c.get("root"))
md = get(f"/api/ruleChain/{root['id']['id']}/metadata")
sw = next(i for i, n in enumerate(md["nodes"]) if n["type"].endswith("TbMsgTypeSwitchNode"))
idx = next((i for i, n in enumerate(md["nodes"]) if n["name"] == INPUT_NODE_NAME), None)
if idx is None:
    md["nodes"].append(node("flow.TbRuleChainInputNode", INPUT_NODE_NAME,
        {"ruleChainId": rc["id"]["id"], "forwardMsgToDefaultRuleChain": False}, 0,
        md["nodes"][sw]["additionalInfo"]["layoutX"] + 300, md["nodes"][sw]["additionalInfo"]["layoutY"] + 400,
        "IncuTwin: copia de telemetría/atributos/actividad hacia la rama de estado del gemelo. El filtro incutwin_enabled está dentro."))
    idx = len(md["nodes"]) - 1
    for t in ["Post telemetry", "Post attributes", "Activity Event", "Inactivity Event"]:
        md["connections"].append({"fromIndex": sw, "toIndex": idx, "type": t})
    post("/api/ruleChain/metadata", md)
    print("raíz", root["name"], ": añadido nodo", INPUT_NODE_NAME)
else:
    print("raíz ya tiene el nodo", INPUT_NODE_NAME)

# 3. habilitar incubadoras
args = sys.argv[1:]
names = [args[i + 1] for i, a in enumerate(args) if a == "--enable"]
for name in names:
    devs = get(f"/api/tenant/devices?pageSize=10&page=0&textSearch={name}")["data"]
    dev = next((d for d in devs if d["name"] == name), None) or (devs[0] if devs else None)
    if not dev: print("no encontrado:", name); continue
    attrs = {"incutwin_enabled": True, "inactivityTimeout": 180000}
    for a in args:
        if a.startswith("--attr="):  # p. ej. --attr=itw_t_parents_min=5
            k, v = a[7:].split("=", 1)
            attrs[k] = int(v) if v.isdigit() else (v == "true") if v in ("true", "false") else v
    post(f"/api/plugins/telemetry/DEVICE/{dev['id']['id']}/SERVER_SCOPE", attrs)
    print("habilitada", dev["name"], dev["id"]["id"], attrs)
print("OK")
