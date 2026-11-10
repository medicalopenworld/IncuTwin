#!/usr/bin/env python3
"""Despliega el historial de estancias por panel (spec tb-stay-history) en ThingsBoard.

Idempotente: se puede relanzar; actualiza lo que ya exista. Solo toca la cadena
IncuTwin_history y una rama de la cadena IncuTwin (paneles).

  python tb/tb_apply_history.py [--dry-run]

Orden de despliegue completo (cada paso con OK, es el servidor real):
  1. python tools/broker_bootstrap.py          # ACL thingsboard -> incutwin/+/cmd/history
  2. python tb/tb_apply_history.py             # esta cadena + rama de sync en IncuTwin
  3. python tb/tb_apply_incutwin_state.py      # rama de eventos de estancia -> IncuTwin_history
  4. python tb/tb_apply_hand_hold.py           # rama de "Coge mi mano" aceptado -> IncuTwin_history
  5. python tools/pair.py ITW-DEEF3C 353       # relacion MirroredBy + itw_panels + sync
Credenciales como tb_apply_incutwin_state.py: tools/factory/secrets/thingsboard.env y, para el
nodo MQTT, MQTT_TB_USER/MQTT_TB_PASS de tools/factory/secrets/broker.env. Nada se imprime.
--dry-run: construye todo sin red ni credenciales y muestra el plan.
"""
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).parent
sys.path.insert(0, str(HERE.parent / "tools"))

CHAIN_NAME = "IncuTwin_history"
PANEL_CHAIN = "IncuTwin"
BROKER_HOST = "mosquitto"
BROKER_PORT = 1883
# Otro client id que el nodo MQTT del estado: dos conexiones con el mismo id se echarian una a otra.
MQTT_CLIENT_ID = "thingsboard-history"

INCUNEST_SERVER_ATTRS = ["itw_state", "country", "itw_show_name", "itw_panel_count", "itw_panels"]
INCUNEST_CLIENT_ATTRS = ["baby_name"]


def node(t, name, cfg, ver, x, y, desc=""):
    return {"type": f"org.thingsboard.rule.engine.{t}", "name": name, "configuration": cfg,
            "configurationVersion": ver, "additionalInfo": {"layoutX": x, "layoutY": y, "description": desc},
            "debugSettings": None}


def js_transform(name, script, x, y, desc=""):
    return node("transform.TbTransformMsgNode", name, {"scriptLang": "JS", "jsScript": script, "tbelScript": "return msg;"}, 0, x, y, desc)


def js_filter(name, script, x, y):
    return node("filter.TbJsFilterNode", name, {"scriptLang": "JS", "jsScript": script, "tbelScript": "return false;"}, 0, x, y)


def js_switch(name, script, x, y):
    return node("filter.TbJsSwitchNode", name, {"scriptLang": "JS", "jsScript": script, "tbelScript": script}, 0, x, y)


def get_attrs(name, server, client, x, y, desc=""):
    return node("metadata.TbGetAttributesNode", name,
                {"tellFailureIfAbsent": False, "clientAttributeNames": client, "sharedAttributeNames": [],
                 "serverAttributeNames": server, "latestTsKeyNames": [], "getLatestValueWithTs": False,
                 "fetchTo": "METADATA"}, 1, x, y, desc)


def to_panel_by_name(name, x, y):
    """Cambia el originador al device del panel cuyo nombre va en msg.panel."""
    return node("transform.TbChangeOriginatorNode", name,
                {"originatorSource": "ENTITY", "entityType": "DEVICE", "entityNamePattern": "$[panel]",
                 "relationsQuery": None}, 1, x, y, "Panel desconocido → Failure (no conectado).")


def to_incunest(name, x, y):
    return node("transform.TbChangeOriginatorNode", name,
                {"originatorSource": "RELATED", "entityType": None, "entityNamePattern": None,
                 "relationsQuery": {"direction": "TO", "maxLevel": 1, "fetchLastLevelOnly": False,
                                    "filters": [{"relationType": "MirroredBy", "entityTypes": ["DEVICE"]}]}},
                1, x, y, "Relación IncuNest —MirroredBy→ panel. Sin relación → Failure: sync sin IncuNest.")


def chain_input(name, rc_id, x, y):
    return node("flow.TbRuleChainInputNode", name, {"ruleChainId": rc_id, "forwardMsgToDefaultRuleChain": False}, 0, x, y)


def input_script(mode):
    src = (HERE / "itw_history_input.js").read_text(encoding="utf-8")
    assert "'__MODE__'" in src
    return src.replace("'__MODE__'", json.dumps(mode))


def history_chain_metadata(rc_id, mqtt_user, mqtt_pass):
    nodes = [
        get_attrs("itw_history del panel", ["itw_history", "itw_utc_offset_min"], [], 150, 200,
                  "Originator = panel. Atributo ausente = historial vacío."),
        js_transform("historial (itw_history.js)", (HERE / "itw_history.js").read_text(encoding="utf-8"), 400, 200,
                     "spec tb-stay-history. SAVE → server attr; PUBLISH → mensaje al panel."),
        js_switch("switch salida", "return [metadata.itw_out];", 650, 200),
        node("telemetry.TbMsgAttributesNode", "guardar itw_history (server)",
             {"scope": "SERVER_SCOPE", "notifyDevice": False, "sendAttributesUpdatedNotification": False,
              "updateAttributesOnlyOnValueChange": False, "processingSettings": {"type": "ON_EVERY_MESSAGE"}},
             3, 950, 100, "Sin notificación de 'Attributes Updated': no reentra en la cadena del panel."),
        node("mqtt.TbMqttNode", "→ broker: incutwin/ID/cmd/history",
             {"topicPattern": "incutwin/${itw_client}/cmd/history", "host": BROKER_HOST, "port": BROKER_PORT,
              "connectTimeoutSec": 10, "clientId": MQTT_CLIENT_ID, "appendClientIdSuffix": False,
              "retainedMessage": True, "cleanSession": True, "ssl": False, "parseToPlainText": False,
              "protocolVersion": "MQTT_3_1_1",
              "credentials": {"type": "basic", "username": mqtt_user, "password": mqtt_pass}}, 2, 950, 300,
             "Retenido, QoS 1: el panel lo recibe al conectar. ACL: thingsboard publishClientSend incutwin/+/cmd/history."),
    ]
    conns = [
        {"fromIndex": 0, "toIndex": 1, "type": "Success"},
        {"fromIndex": 1, "toIndex": 2, "type": "Success"},
        {"fromIndex": 2, "toIndex": 3, "type": "SAVE"},
        {"fromIndex": 2, "toIndex": 4, "type": "PUBLISH"},
    ]
    return {"ruleChainId": rc_id, "firstNodeIndex": 0, "nodes": nodes, "connections": conns, "ruleChainConnections": None}


SYNC_PARSE = """var r = {};
try { r = JSON.parse(msg.itw_history_sync); } catch (e) { r = {}; }
var m = {}; for (var k in metadata) { m[k] = metadata[k]; }
m.itw_panel = String(r.panel || '');
return { msg: { kind: r.kind === 'unpaired' ? 'unpaired' : 'sync', panel: m.itw_panel }, metadata: m, msgType: 'ITW_HISTORY' };"""


def panel_sync_branch(hist_id):
    """Rama de la cadena IncuTwin: pair.py/unpair.py escriben el server attr itw_history_sync
    ({"kind":"sync"|"unpaired","panel":"ITW-…","t":ms}) y eso llega como Attributes Updated."""
    nodes = [
        js_filter("¿itw_history_sync?", "return msg.itw_history_sync !== undefined;", 400, 650),
        js_transform("sync: leer petición", SYNC_PARSE, 650, 650),
        js_switch("sync o desemparejado", "return [msg.kind];", 900, 650),
        to_incunest("→ IncuNest emparejada (sync)", 1150, 600),
        get_attrs("estado de la IncuNest", INCUNEST_SERVER_ATTRS, INCUNEST_CLIENT_ATTRS, 1400, 600),
        js_transform("entrada historial (sync)", input_script("sync"), 1650, 600),
        to_panel_by_name("→ panel (sync)", 1900, 600),
        chain_input("→ IncuTwin_history", hist_id, 2150, 700),
    ]
    conns = [(-1, 0, "Attributes Updated"), (0, 1, "True"), (1, 2, "Success"),
             (2, 7, "unpaired"), (2, 3, "sync"), (3, 4, "Success"), (4, 5, "Success"), (5, 6, "Success"),
             (6, 7, "Success"), (3, 7, "Failure")]
    return nodes, conns


def append_branch(tb, chain_name, branch_nodes, branch_conns):
    """Añade la rama si su primer nodo (por nombre) no está; si está, actualiza las configuraciones.
    branch_conns usa índices de branch_nodes; -1 = Message Type Switch de la cadena."""
    c = next((c for c in tb.page_all("/api/ruleChains") if c["name"] == chain_name), None)
    if not c:
        sys.exit(f"no existe la rule chain {chain_name}")
    md = tb.get(f"/api/ruleChain/{c['id']['id']}/metadata")
    names = {n["name"]: i for i, n in enumerate(md["nodes"])}
    sw = next(i for i, n in enumerate(md["nodes"]) if n["type"].endswith("TbMsgTypeSwitchNode"))
    if branch_nodes[0]["name"] in names:
        for bn in branch_nodes:
            if bn["name"] in names:
                md["nodes"][names[bn["name"]]]["configuration"] = bn["configuration"]
        tb.post("/api/ruleChain/metadata", md)
        print(chain_name, ": rama ya presente, configuración actualizada")
        return
    base = len(md["nodes"])
    md["nodes"].extend(branch_nodes)
    for f, t, ty in branch_conns:
        md["connections"].append({"fromIndex": sw if f == -1 else base + f, "toIndex": base + t, "type": ty})
    tb.post("/api/ruleChain/metadata", md)
    print(chain_name, ": añadida rama", branch_nodes[0]["name"])


def dry_run():
    md = history_chain_metadata({"id": "DRY", "entityType": "RULE_CHAIN"}, "thingsboard", "***")
    print(f"[dry-run] cadena {CHAIN_NAME}:")
    for i, n in enumerate(md["nodes"]):
        print(f"  {i} {n['type'].rsplit('.', 1)[1]:28} {n['name']}")
    for c in md["connections"]:
        print(f"     {c['fromIndex']} -{c['type']}-> {c['toIndex']}")
    nodes, conns = panel_sync_branch("DRY")
    print(f"[dry-run] rama nueva en {PANEL_CHAIN}:")
    for i, n in enumerate(nodes):
        print(f"  b{i} {n['type'].rsplit('.', 1)[1]:28} {n['name']}")
    for f, t, ty in conns:
        print(f"     {'switch' if f == -1 else 'b%d' % f} -{ty}-> b{t}")
    for mode in ("event", "hold", "sync"):
        compile_check = input_script(mode)
        assert "'__MODE__'" not in compile_check
    print("[dry-run] scripts de entrada: event, hold, sync OK. Nada enviado.")


def main():
    try:  # consola de Windows (cp1252): que los nombres con flechas no rompan la salida
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    if "--dry-run" in sys.argv:
        dry_run()
        return
    from broker_mqtt import load_env as load_broker_env  # noqa: E402
    from tb_api import TB  # noqa: E402
    benv = load_broker_env()
    mqtt_user = benv.get("MQTT_TB_USER", "thingsboard")
    mqtt_pass = benv.get("MQTT_TB_PASS", "")
    if not mqtt_pass:
        sys.exit("MQTT_TB_PASS vacio en broker.env: ejecuta tools/broker_bootstrap.py")
    tb = TB()
    rc = next((c for c in tb.page_all("/api/ruleChains") if c["name"] == CHAIN_NAME), None)
    if not rc:
        rc = tb.post("/api/ruleChain", {"name": CHAIN_NAME, "type": "CORE", "root": False, "debugMode": False,
                     "additionalInfo": {"description": "Historial de estancias por panel (spec tb-stay-history). "
                                        "Originator = panel. La llaman IncuNest_IncuTwin_state (estancias), "
                                        "IncuTwin_hand_hold (Coge mi mano aceptado) e IncuTwin (sync de pair.py)."}})
        print("creada rule chain", CHAIN_NAME)
    tb.post("/api/ruleChain/metadata", history_chain_metadata(rc["id"], mqtt_user, mqtt_pass))
    print("metadata actualizada:", CHAIN_NAME, rc["id"]["id"])
    nodes, conns = panel_sync_branch(rc["id"]["id"])
    append_branch(tb, PANEL_CHAIN, nodes, conns)
    print("OK. Ahora relanza tb_apply_incutwin_state.py y tb_apply_hand_hold.py para conectar sus ramas.")


if __name__ == "__main__":
    main()
