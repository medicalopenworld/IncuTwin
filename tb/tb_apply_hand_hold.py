#!/usr/bin/env python3
"""Despliega "Coge mi mano" (Contrato IncuTwin v0.2 §3.5) en ThingsBoard.

Idempotente: se puede relanzar; actualiza lo que ya exista.
Uso:
  TB_URL=https://mon.medicalopenworld.org TB_API_KEY=... python3 tb_apply_hand_hold.py [--map SN=DEVICE ...]
  (o TB_USER/TB_PASS en lugar de TB_API_KEY)
Hace:
  1. Crea/actualiza la rule chain "IncuTwin_hand_hold" (originator = IncuNest): valida
     incutwin_enabled e itw_baby_state ∈ {in, parents} y escribe el shared attr
     hand_hold_until en la IncuNest y en su panel MirroredBy. Audita en itw_hand_hold /
     itw_hand_hold_rejected (telemetría de la IncuNest, TTL 30 d).
  2. Rule chain "IncuTwin" (paneles): telemetría {"hand_hold":1} -> IncuNest emparejada ->
     IncuTwin_hand_hold.
  3. Rule chain "IncuTwin_bridge" (app): telemetría {"incubator_id": SN, "hand_hold_minutes": n}
     -> IncuNest (mapa SN->device en el server attr itw_sn_map del device bridge, o
     "IncuNest-<SN>" por defecto) -> IncuTwin_hand_hold.
  4. --map SN=DEVICE añade entradas a itw_sn_map (p. ej. --map 1=IncuNest-1_2: el nombre
     lleva sufijo de colisión y no se deduce del SN).
Solo toca las rule chains IncuTwin*, y el device IncuTwin_cloud_bridge.
"""
import json, os, sys, pathlib, requests

TB = os.environ.get("TB_URL", "https://mon.medicalopenworld.org").rstrip("/")
HERE = pathlib.Path(__file__).parent
HH_CHAIN = "IncuTwin_hand_hold"
PANEL_CHAIN = "IncuTwin"
BRIDGE_CHAIN = "IncuTwin_bridge"
BRIDGE_DEVICE = "IncuTwin_cloud_bridge"

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

def js_transform(name, script, x, y, desc=""):
    return node("transform.TbTransformMsgNode", name, {"scriptLang": "JS", "jsScript": script, "tbelScript": "return msg;"}, 0, x, y, desc)

def js_filter(name, script, x, y):
    return node("filter.TbJsFilterNode", name, {"scriptLang": "JS", "jsScript": script, "tbelScript": "return false;"}, 0, x, y)

def save_shared(name, x, y):
    return node("telemetry.TbMsgAttributesNode", name,
                {"scope": "SHARED_SCOPE", "notifyDevice": True, "sendAttributesUpdatedNotification": False,
                 "updateAttributesOnlyOnValueChange": False, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 3, x, y)

def related(name, direction, x, y, desc=""):
    return node("transform.TbChangeOriginatorNode", name,
                {"originatorSource": "RELATED", "entityType": None, "entityNamePattern": None,
                 "relationsQuery": {"direction": direction, "maxLevel": 1, "fetchLastLevelOnly": False,
                                    "filters": [{"relationType": "MirroredBy", "entityTypes": ["DEVICE"]}]}}, 1, x, y, desc)

def chain_input(name, rc_id, x, y):
    return node("flow.TbRuleChainInputNode", name, {"ruleChainId": rc_id, "forwardMsgToDefaultRuleChain": False}, 0, x, y)

chains = get("/api/ruleChains?pageSize=200&page=0")["data"]
def chain_by_name(n): return next((c for c in chains if c["name"] == n), None)

# 1. sub-cadena IncuTwin_hand_hold
rc = chain_by_name(HH_CHAIN)
if not rc:
    rc = post("/api/ruleChain", {"name": HH_CHAIN, "type": "CORE", "root": False, "debugMode": False,
              "additionalInfo": {"description": "Coge mi mano (Contrato IncuTwin v0.2 §3.5). Originator = IncuNest. La llaman IncuTwin (panel) e IncuTwin_bridge (app)."}})
    print("creada rule chain", HH_CHAIN)
hh_id = rc["id"]["id"]
nodes = [
    node("metadata.TbGetAttributesNode", "incutwin_enabled + itw_baby_state",
         {"tellFailureIfAbsent": False, "clientAttributeNames": [], "sharedAttributeNames": [],
          "serverAttributeNames": ["incutwin_enabled", "itw_baby_state"], "latestTsKeyNames": [],
          "getLatestValueWithTs": False, "fetchTo": "METADATA"}, 1, 150, 200),
    js_transform("validar + hand_hold_until", (HERE / "itw_hand_hold.js").read_text(encoding="utf-8"), 400, 200,
                 "Solo con bebé in/parents. Minutos 1..30 (5 por defecto)."),
    node("filter.TbJsSwitchNode", "switch salida",
         {"scriptLang": "JS", "jsScript": "return [metadata.itw_out];", "tbelScript": "return [metadata.itw_out];"}, 0, 650, 200),
    save_shared("hand_hold_until → IncuNest", 950, 100),
    related("→ panel MirroredBy", "FROM", 950, 250, "Sin panel emparejado → Failure (no conectado): normal."),
    save_shared("hand_hold_until → panel", 1250, 250),
    node("telemetry.TbMsgTimeseriesNode", "auditoría (TTL 30d)",
         {"defaultTTL": 2592000, "useServerTs": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 1, 950, 400),
]
conns = [
    {"fromIndex": 0, "toIndex": 1, "type": "Success"},
    {"fromIndex": 1, "toIndex": 2, "type": "Success"},
    {"fromIndex": 2, "toIndex": 3, "type": "HH"},
    {"fromIndex": 2, "toIndex": 4, "type": "HH"},
    {"fromIndex": 4, "toIndex": 5, "type": "Success"},
    {"fromIndex": 2, "toIndex": 6, "type": "LOG"},
    {"fromIndex": 2, "toIndex": 6, "type": "REJECT"},
]
post("/api/ruleChain/metadata", {"ruleChainId": rc["id"], "firstNodeIndex": 0, "nodes": nodes, "connections": conns, "ruleChainConnections": None})
print("metadata actualizada:", HH_CHAIN, hh_id)

def append_branch(chain_name, branch_nodes, branch_conns):
    """Añade branch_nodes a la cadena si su primer nodo (por nombre) no está ya.
    branch_conns usa índices relativos a branch_nodes; -1 = Message Type Switch."""
    c = chain_by_name(chain_name)
    md = get(f"/api/ruleChain/{c['id']['id']}/metadata")
    names = {n["name"]: i for i, n in enumerate(md["nodes"])}
    sw = next(i for i, n in enumerate(md["nodes"]) if n["type"].endswith("TbMsgTypeSwitchNode"))
    if branch_nodes[0]["name"] in names:
        # actualizar configuración de los nodos existentes (scripts incluidos)
        for bn in branch_nodes:
            if bn["name"] in names: md["nodes"][names[bn["name"]]]["configuration"] = bn["configuration"]
        post("/api/ruleChain/metadata", md)
        print(chain_name, ": rama ya presente, configuración actualizada")
        return
    base = len(md["nodes"])
    md["nodes"].extend(branch_nodes)
    for f, t, ty in branch_conns:
        md["connections"].append({"fromIndex": sw if f == -1 else base + f, "toIndex": base + t, "type": ty})
    post("/api/ruleChain/metadata", md)
    print(chain_name, ": añadida rama", branch_nodes[0]["name"])

# 2. paneles
append_branch(PANEL_CHAIN, [
    js_filter("¿hand_hold?", "return msg.hand_hold !== undefined;", 400, 450),
    js_transform("hand_hold desde panel",
                 "return { msg: { hand_hold_minutes: 5, source: 'panel', panel: metadata.deviceName }, metadata: metadata, msgType: msgType };",
                 650, 450),
    related("→ IncuNest emparejada", "TO", 900, 450, "Relación IncuNest —MirroredBy→ panel."),
    chain_input("→ IncuTwin_hand_hold", hh_id, 1150, 450),
], [(-1, 0, "Post telemetry"), (0, 1, "True"), (1, 2, "Success"), (2, 3, "Success")])

# 3. bridge (app)
BRIDGE_SCRIPT = """var map = {};
try { map = JSON.parse(metadata.ss_itw_sn_map || '{}'); } catch (e) { map = {}; }
var sn = String(msg.incubator_id);
var m = {}; for (var k in metadata) m[k] = metadata[k];
m.itw_target = map[sn] || ('IncuNest-' + sn);
return { msg: { hand_hold_minutes: msg.hand_hold_minutes, source: 'app' }, metadata: m, msgType: msgType };"""
append_branch(BRIDGE_CHAIN, [
    js_filter("¿incubator_id?", "return msg.incubator_id !== undefined && msg.incubator_id !== null;", 400, 450),
    node("metadata.TbGetAttributesNode", "itw_sn_map",
         {"tellFailureIfAbsent": False, "clientAttributeNames": [], "sharedAttributeNames": [],
          "serverAttributeNames": ["itw_sn_map"], "latestTsKeyNames": [], "getLatestValueWithTs": False,
          "fetchTo": "METADATA"}, 1, 650, 450),
    js_transform("SN → device", BRIDGE_SCRIPT, 900, 450),
    node("transform.TbChangeOriginatorNode", "→ IncuNest por nombre",
         {"originatorSource": "ENTITY", "entityType": "DEVICE", "entityNamePattern": "${itw_target}", "relationsQuery": None},
         1, 1150, 450, "SN desconocido → Failure (no conectado)."),
    chain_input("→ IncuTwin_hand_hold", hh_id, 1400, 450),
], [(-1, 0, "Post telemetry"), (0, 1, "True"), (1, 2, "Success"), (2, 3, "Success"), (3, 4, "Success")])

# 4. mapa SN -> device en el bridge
args = sys.argv[1:]
maps = [args[i + 1] for i, a in enumerate(args) if a == "--map"]
if maps:
    dev = get(f"/api/tenant/devices?deviceName={BRIDGE_DEVICE}")
    cur = get(f"/api/plugins/telemetry/DEVICE/{dev['id']['id']}/values/attributes/SERVER_SCOPE?keys=itw_sn_map")
    m = json.loads(cur[0]["value"]) if cur else {}
    for kv in maps:
        k, v = kv.split("=", 1); m[k] = v
    post(f"/api/plugins/telemetry/DEVICE/{dev['id']['id']}/SERVER_SCOPE", {"itw_sn_map": json.dumps(m)})
    print("itw_sn_map =", m)
print("OK")
