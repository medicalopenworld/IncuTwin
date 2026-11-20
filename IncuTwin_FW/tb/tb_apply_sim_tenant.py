#!/usr/bin/env python3
"""Monta en el tenant de PRUEBAS (Demo) las IncuNest simuladas para el desarrollo de la app.

Solo funciona con TB_ENV_FILE apuntando a las credenciales del tenant de pruebas; se niega a
tocar el tenant de producción. Idempotente: se puede relanzar.

  $env:TB_ENV_FILE = "<ruta>/thingsboard-test.env"
  python tb/tb_apply_sim_tenant.py [--mqtt-env <ruta>/broker-sim.env]

Hace:
  1. Crea/actualiza la rule chain "IncuNest_IncuTwin_state" con la MISMA máquina de estados que
     producción (tb/itw_state_machine.js). Sin --mqtt-env no publica en el broker (el payload sale
     en el log); con --mqtt-env usa ese cliente del broker (MQTT_TB_USER / MQTT_TB_PASS).
  2. Crea/actualiza la rule chain "IncuNest_SIM_root" (guardar telemetría y atributos + rama de
     estado) y el device profile "INCUNEST-SIM" que la usa. No toca la raíz del tenant.
  3. Crea IncuNest-SIM01..03 (SN SIM01..03) con incutwin_enabled, inactividad de 2 min y tiempos
     de canguro / inactividad cortos para poder probarlos.
  4. "Coge mi mano": rule chain IncuTwin_hand_hold (misma lógica que producción) y el device
     IncuTwin_cloud_bridge (perfil INCUTWIN-BRIDGE) para la API de la app.
  5. Guarda los tokens en sim-devices.env (SIM01..03) y next-tb-bridge.env (bridge), junto al
     TB_ENV_FILE. Nada se imprime.
"""
import argparse
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).parent
sys.path.insert(0, str(HERE.parent / "tools"))
sys.path.insert(0, str(HERE))

if not os.environ.get("TB_ENV_FILE"):
    sys.exit("define TB_ENV_FILE con las credenciales del tenant de PRUEBAS")

from tb_api import TB, TB_ENV  # noqa: E402

PROD_EMAILS = {"it@medicalopenworld.org"}
STATE_CHAIN = "IncuNest_IncuTwin_state"
SIM_ROOT = "IncuNest_SIM_root"
PROFILE = "INCUNEST-SIM"
SIMS = [("SIM01", "IncuNest simulada 1", "TG"), ("SIM02", "IncuNest simulada 2", "SN"),
        ("SIM03", "IncuNest simulada 3 (demo / revisión de tiendas)", "ES")]
SIM_ATTRS = {"incutwin_enabled": True, "inactivityTimeout": 120000,
             "itw_t_parents_min": 10, "itw_t_idle_min": 15}

tb = TB()
get, post = tb.get, tb.post


def guard():
    user = get("/api/auth/user")
    email = (user.get("email") or "").lower()
    if email in PROD_EMAILS or user.get("authority") != "TENANT_ADMIN":
        sys.exit(f"esto no parece el tenant de pruebas ({email}); no toco nada")
    total = get("/api/tenant/devices?pageSize=1&page=0").get("totalElements", 0)
    if total > 100:
        sys.exit(f"el tenant tiene {total} devices: parece producción; no toco nada")
    print("tenant de pruebas:", email, "|", total, "devices")


def node(t, name, cfg, ver, x, y, desc=""):
    return {"type": f"org.thingsboard.rule.engine.{t}", "name": name, "configuration": cfg,
            "configurationVersion": ver, "additionalInfo": {"layoutX": x, "layoutY": y, "description": desc},
            "debugSettings": None}


def ensure_chain(chains, name, desc):
    rc = next((c for c in chains if c["name"] == name), None)
    if not rc:
        rc = post("/api/ruleChain", {"name": name, "type": "CORE", "root": False, "debugMode": False,
                                     "additionalInfo": {"description": desc}})
        print("creada rule chain", name)
    return rc


def load_mqtt(path):
    env = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip()
    if not env.get("MQTT_TB_USER") or not env.get("MQTT_TB_PASS"):
        sys.exit(f"faltan MQTT_TB_USER / MQTT_TB_PASS en {path}")
    return env["MQTT_TB_USER"], env["MQTT_TB_PASS"]


def sim_root_metadata(rc_id, state_id):
    proc = {"type": "ON_EVERY_MESSAGE"}
    nodes = [
        node("filter.TbMsgTypeSwitchNode", "Message Type Switch", {"version": 0}, 0, 100, 200),
        node("telemetry.TbMsgTimeseriesNode", "guardar telemetría",
             {"defaultTTL": 2592000, "useServerTs": False, "processingSettings": proc}, 1, 400, 50,
             "TTL 30 días: es un tenant de pruebas."),
        node("telemetry.TbMsgAttributesNode", "guardar client attributes",
             {"scope": "CLIENT_SCOPE", "notifyDevice": False, "sendAttributesUpdatedNotification": False,
              "updateAttributesOnlyOnValueChange": True, "processingSettings": proc}, 3, 400, 150),
        node("flow.TbRuleChainInputNode", "→ IncuTwin state",
             {"ruleChainId": state_id, "forwardMsgToDefaultRuleChain": False}, 0, 400, 300,
             "Misma rama de estado que producción."),
        node("action.TbLogNode", "otros",
             {"scriptLang": "JS", "jsScript": "return msgType;", "tbelScript": "return msgType;"}, 0, 400, 450),
    ]
    conns = [{"fromIndex": 0, "toIndex": 1, "type": "Post telemetry"},
             {"fromIndex": 0, "toIndex": 2, "type": "Post attributes"},
             {"fromIndex": 0, "toIndex": 4, "type": "Other"}]
    for t in ["Post telemetry", "Post attributes", "Activity Event", "Inactivity Event"]:
        conns.append({"fromIndex": 0, "toIndex": 3, "type": t})
    return {"ruleChainId": rc_id, "firstNodeIndex": 0, "nodes": nodes, "connections": conns,
            "ruleChainConnections": None}


def ensure_profile(root_id):
    prof = next((p for p in tb.page_all("/api/deviceProfiles") if p["name"] == PROFILE), None)
    body = prof or {"name": PROFILE, "type": "DEFAULT", "transportType": "DEFAULT", "provisionType": "DISABLED",
                    "profileData": {"configuration": {"type": "DEFAULT"},
                                    "transportConfiguration": {"type": "DEFAULT"},
                                    "provisionConfiguration": {"type": "DISABLED"}, "alarms": None}}
    body["defaultRuleChainId"] = {"entityType": "RULE_CHAIN", "id": root_id}
    body["description"] = "IncuNest simuladas para el desarrollo de la app (tools/incunest_sim.py)."
    prof = post("/api/deviceProfile", body)
    print("device profile", PROFILE, "->", SIM_ROOT)
    return prof["id"]


HH_CHAIN = "IncuTwin_hand_hold"
BRIDGE_CHAIN = "IncuTwin_bridge"
BRIDGE_DEVICE = "IncuTwin_cloud_bridge"
BRIDGE_PROFILE = "INCUTWIN-BRIDGE"
# Igual que tb/tb_apply_hand_hold.py (producción); aquí las cadenas se crean enteras porque en
# el tenant de pruebas no existen IncuTwin ni IncuTwin_bridge.
BRIDGE_SCRIPT = """var map = {};
try { map = JSON.parse(metadata.ss_itw_sn_map || '{}'); } catch (e) { map = {}; }
var sn = String(msg.incubator_id);
var m = {}; for (var k in metadata) { m[k] = metadata[k]; }
m.itw_target = map[sn] || ('IncuNest-' + sn);
return { msg: { hand_hold_minutes: msg.hand_hold_minutes, source: 'app' }, metadata: m, msgType: msgType };"""


def js(t, name, script, x, y, desc=""):
    return node(t, name, {"scriptLang": "JS", "jsScript": script, "tbelScript": "return msg;"}, 0, x, y, desc)


def save_shared(name, x, y):
    return node("telemetry.TbMsgAttributesNode", name,
                {"scope": "SHARED_SCOPE", "notifyDevice": True, "sendAttributesUpdatedNotification": False,
                 "updateAttributesOnlyOnValueChange": False, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 3, x, y)


def setup_hand_hold(chains):
    """Coge mi mano: cadena IncuTwin_hand_hold (misma lógica que producción, tb/itw_hand_hold.js) y
    device bridge con su propia cadena. Devuelve el token del bridge."""
    hh = ensure_chain(chains, HH_CHAIN, "Coge mi mano (copia de producción). Originator = IncuNest.")
    nodes = [
        node("metadata.TbGetAttributesNode", "incutwin_enabled + itw_baby_state",
             {"tellFailureIfAbsent": False, "clientAttributeNames": [], "sharedAttributeNames": [],
              "serverAttributeNames": ["incutwin_enabled", "itw_baby_state"], "latestTsKeyNames": [],
              "getLatestValueWithTs": False, "fetchTo": "METADATA"}, 1, 150, 200),
        js("transform.TbTransformMsgNode", "validar + hand_hold_until",
           (HERE / "itw_hand_hold.js").read_text(encoding="utf-8"), 400, 200, "Solo con bebé in/parents. Minutos 1..30."),
        js("filter.TbJsSwitchNode", "switch salida", "return [metadata.itw_out];", 650, 200),
        save_shared("hand_hold_until → IncuNest", 950, 100),
        node("telemetry.TbMsgTimeseriesNode", "auditoría (TTL 30d)",
             {"defaultTTL": 2592000, "useServerTs": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 1, 950, 300),
    ]
    conns = [{"fromIndex": 0, "toIndex": 1, "type": "Success"}, {"fromIndex": 1, "toIndex": 2, "type": "Success"},
             {"fromIndex": 2, "toIndex": 3, "type": "HH"}, {"fromIndex": 2, "toIndex": 4, "type": "LOG"},
             {"fromIndex": 2, "toIndex": 4, "type": "REJECT"}]
    post("/api/ruleChain/metadata", {"ruleChainId": hh["id"], "firstNodeIndex": 0, "nodes": nodes,
                                     "connections": conns, "ruleChainConnections": None})
    print("rule chain", HH_CHAIN, "actualizada")

    br = ensure_chain(chains, BRIDGE_CHAIN, "Bridge de la app: {incubator_id, hand_hold_minutes} -> IncuNest -> Coge mi mano.")
    nodes = [
        node("filter.TbMsgTypeSwitchNode", "Message Type Switch", {"version": 0}, 0, 100, 200),
        node("telemetry.TbMsgTimeseriesNode", "guardar petición (TTL 30d)",
             {"defaultTTL": 2592000, "useServerTs": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 1, 400, 50),
        js("filter.TbJsFilterNode", "¿incubator_id?",
           "return msg.incubator_id !== undefined && msg.incubator_id !== null;", 400, 200),
        node("metadata.TbGetAttributesNode", "itw_sn_map",
             {"tellFailureIfAbsent": False, "clientAttributeNames": [], "sharedAttributeNames": [],
              "serverAttributeNames": ["itw_sn_map"], "latestTsKeyNames": [], "getLatestValueWithTs": False,
              "fetchTo": "METADATA"}, 1, 650, 200),
        js("transform.TbTransformMsgNode", "SN → device", BRIDGE_SCRIPT, 900, 200),
        node("transform.TbChangeOriginatorNode", "→ IncuNest por nombre",
             {"originatorSource": "ENTITY", "entityType": "DEVICE", "entityNamePattern": "${itw_target}",
              "relationsQuery": None}, 1, 1150, 200, "SN desconocido → Failure."),
        node("flow.TbRuleChainInputNode", "→ IncuTwin_hand_hold",
             {"ruleChainId": hh["id"]["id"], "forwardMsgToDefaultRuleChain": False}, 0, 1400, 200),
    ]
    conns = [{"fromIndex": 0, "toIndex": 1, "type": "Post telemetry"}, {"fromIndex": 0, "toIndex": 2, "type": "Post telemetry"},
             {"fromIndex": 2, "toIndex": 3, "type": "True"}, {"fromIndex": 3, "toIndex": 4, "type": "Success"},
             {"fromIndex": 4, "toIndex": 5, "type": "Success"}, {"fromIndex": 5, "toIndex": 6, "type": "Success"}]
    post("/api/ruleChain/metadata", {"ruleChainId": br["id"], "firstNodeIndex": 0, "nodes": nodes,
                                     "connections": conns, "ruleChainConnections": None})
    print("rule chain", BRIDGE_CHAIN, "actualizada")

    prof = next((p for p in tb.page_all("/api/deviceProfiles") if p["name"] == BRIDGE_PROFILE), None)
    body = prof or {"name": BRIDGE_PROFILE, "type": "DEFAULT", "transportType": "DEFAULT", "provisionType": "DISABLED",
                    "profileData": {"configuration": {"type": "DEFAULT"}, "transportConfiguration": {"type": "DEFAULT"},
                                    "provisionConfiguration": {"type": "DISABLED"}, "alarms": None}}
    body["defaultRuleChainId"] = {"entityType": "RULE_CHAIN", "id": br["id"]["id"]}
    body["description"] = "Device bridge de la app (Coge mi mano)."
    prof = post("/api/deviceProfile", body)
    dev = next((d for d in tb.page_all(f"/api/tenant/devices?textSearch={BRIDGE_DEVICE}") if d["name"] == BRIDGE_DEVICE), None)
    if not dev:
        dev = post("/api/device", {"name": BRIDGE_DEVICE, "label": "Bridge de la app (pruebas)", "deviceProfileId": prof["id"]})
        print("creado", BRIDGE_DEVICE)
    elif dev["deviceProfileId"]["id"] != prof["id"]["id"]:
        dev["deviceProfileId"] = prof["id"]
        dev = post("/api/device", dev)
    return get(f"/api/device/{dev['id']['id']}/credentials")["credentialsId"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mqtt-env", help="fichero con MQTT_TB_USER / MQTT_TB_PASS del cliente del broker para Demo")
    args = ap.parse_args()
    guard()
    import tb_apply_incutwin_state as state_mod  # mismo TB_ENV_FILE

    mqtt = load_mqtt(args.mqtt_env) if args.mqtt_env else False
    chains = list(tb.page_all("/api/ruleChains"))
    st = ensure_chain(chains, STATE_CHAIN, "Rama 'estado del gemelo' (copia de producción) para las IncuNest simuladas.")
    post("/api/ruleChain/metadata", state_mod.chain_metadata(st["id"], None, mqtt))
    print("rule chain", STATE_CHAIN, "actualizada |", "broker: " + mqtt[0] if mqtt else "sin broker (log)")

    root = ensure_chain(chains, SIM_ROOT, "Raíz de las IncuNest simuladas (perfil INCUNEST-SIM).")
    post("/api/ruleChain/metadata", sim_root_metadata(root["id"], st["id"]["id"]))
    print("rule chain", SIM_ROOT, "actualizada")
    prof_id = ensure_profile(root["id"]["id"])

    tokens = {}
    for sn, label, country in SIMS:
        name = f"IncuNest-{sn}"
        dev = next((d for d in tb.page_all(f"/api/tenant/devices?textSearch={name}") if d["name"] == name), None)
        if not dev:
            dev = post("/api/device", {"name": name, "label": label, "deviceProfileId": prof_id})
            print("creado", name)
        elif dev["deviceProfileId"]["id"] != prof_id["id"]:
            dev["deviceProfileId"] = prof_id
            dev = post("/api/device", dev)
        post(f"/api/plugins/telemetry/DEVICE/{dev['id']['id']}/SERVER_SCOPE", dict(SIM_ATTRS, country=country))
        cred = get(f"/api/device/{dev['id']['id']}/credentials")
        tokens[sn] = cred["credentialsId"]
        print("listo", name, "| país", country, "|", SIM_ATTRS)

    bridge_token = setup_hand_hold(chains)
    (pathlib.Path(TB_ENV).parent / "next-tb-bridge.env").write_text(
        "# ThingsBoard de pruebas - device bridge de 'Coge mi mano' (solo puede mandar telemetría a sí mismo)\n"
        f"TB_URL={tb.url}\nTB_BRIDGE_TOKEN={bridge_token}\n"
        "# POST $TB_URL/api/v1/$TB_BRIDGE_TOKEN/telemetry  {\"incubator_id\":\"SIM01\",\"hand_hold_minutes\":5}\n",
        encoding="utf-8")
    print("token del bridge guardado en next-tb-bridge.env")

    out = pathlib.Path(TB_ENV).parent / "sim-devices.env"
    out.write_text("# Tokens de las IncuNest simuladas (tenant de pruebas). Para tools/incunest_sim.py.\n"
                   + "".join(f"{sn}={tok}\n" for sn, tok in tokens.items()), encoding="utf-8")
    print("tokens guardados en", out)
    print("OK")


if __name__ == "__main__":
    main()
