#!/usr/bin/env python3
"""Despliega la rama "estado del gemelo" (Contrato IncuTwin v0.2 §3.4) y su salida al broker
MQTT (docs/BROKER-MQTT-CONTEXT.md §3/§5) en ThingsBoard.

Idempotente: se puede relanzar; actualiza la rule chain si ya existe.
Credenciales: tools/factory/secrets/thingsboard.env (TB_API_KEY o TB_USER/TB_PASS) y, para el
nodo MQTT, MQTT_TB_USER/MQTT_TB_PASS de tools/factory/secrets/broker.env (cliente `thingsboard`
del broker, creado por tools/broker_bootstrap.py). Nada se imprime.

Uso:
  python tb/tb_apply_incutwin_state.py [--enable TEST-incubadora] [--attr=itw_show_name=true]
Hace:
  1. Crea/actualiza la rule chain "IncuNest_IncuTwin_state" (maquina de estados, server attrs,
     shared attrs a paneles legados, telemetria itw_event, webhook pendiente y nodo MQTT al broker).
  2. Anade a la rule chain raiz un nodo "rule chain" hacia ella, conectado desde el Message Type
     Switch en: Post telemetry, Post attributes, Activity Event, Inactivity Event.
  3. Pone incutwin_enabled=true e inactivityTimeout=180000 en las incubadoras de --enable.
No toca ningun otro nodo ni dispositivo.
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).parent
sys.path.insert(0, str(HERE.parent / "tools"))
from broker_mqtt import load_env as load_broker_env  # noqa: E402
from tb_api import TB  # noqa: E402

CHAIN_NAME = "IncuNest_IncuTwin_state"
INPUT_NODE_NAME = "→ IncuTwin state"
BROKER_HOST = "mosquitto"   # red Docker interna del VPS (no es 127.0.0.1 ni el 1883 publico)
BROKER_PORT = 1883

tb = TB()
get, post = tb.get, tb.post


def node(t, name, cfg, ver, x, y, desc=""):
    return {"type": f"org.thingsboard.rule.engine.{t}", "name": name, "configuration": cfg,
            "configurationVersion": ver, "additionalInfo": {"layoutX": x, "layoutY": y, "description": desc},
            "debugSettings": None}


def chain_metadata(rc_id):
    sm = (HERE / "itw_state_machine.js").read_text(encoding="utf-8")
    ev2ts = (HERE / "itw_event_to_ts.js").read_text(encoding="utf-8")
    benv = load_broker_env()
    tb_user = benv.get("MQTT_TB_USER", "thingsboard")
    tb_pass = benv.get("MQTT_TB_PASS", "")
    if not tb_pass:
        sys.exit("MQTT_TB_PASS vacio en broker.env: ejecuta tools/broker_bootstrap.py")
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
           "Contrato v0.2 §3.4 + broker §3. Emite ITW_STATE (server attrs), ITW_PANEL (shared attrs a paneles legados), ITW_EVENT (telemetría/webhook) e ITW_BROKER (MQTT retenido)."),
      node("filter.TbJsSwitchNode", "switch salida",
           {"scriptLang": "JS", "jsScript": "return [metadata.itw_out];", "tbelScript": "return [metadata.itw_out];"}, 0, 1150, 200),
      node("telemetry.TbMsgAttributesNode", "guardar itw_state (server)",
           {"scope": "SERVER_SCOPE", "notifyDevice": False, "sendAttributesUpdatedNotification": False,
            "updateAttributesOnlyOnValueChange": True, "processingSettings": {"type": "ON_EVERY_MESSAGE"}}, 3, 1450, 50),
      node("transform.TbChangeOriginatorNode", "→ paneles MirroredBy",
           {"originatorSource": "RELATED", "entityType": None, "entityNamePattern": None,
            "relationsQuery": {"direction": "FROM", "maxLevel": 1, "fetchLastLevelOnly": False,
                               "filters": [{"relationType": "MirroredBy", "entityTypes": ["DEVICE"]}]}}, 1, 1450, 200,
           "Paneles legados (firmware Arduino). Sin panel emparejado → Failure (no conectado): normal."),
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
      node("mqtt.TbMqttNode", "→ broker IncuTwin (mosquitto)",
           {"topicPattern": "incubators/${itw_incubator_id}/state", "host": BROKER_HOST, "port": BROKER_PORT,
            "connectTimeoutSec": 10, "clientId": tb_user, "appendClientIdSuffix": False, "retainedMessage": True,
            "cleanSession": True, "ssl": False, "parseToPlainText": False, "protocolVersion": "MQTT_3_1_1",
            "credentials": {"type": "basic", "username": tb_user, "password": tb_pass}}, 2, 1450, 650,
           "docs/BROKER.md §3/§5: estado completo retenido en incubators/SN/state, una vez por evento. "
           "Red Docker interna, sin TLS. Credenciales del cliente 'thingsboard' del broker."),
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
      {"fromIndex": 4, "toIndex": 11, "type": "ITW_BROKER"},
    ]
    return {"ruleChainId": rc_id, "firstNodeIndex": 0, "nodes": nodes, "connections": conns, "ruleChainConnections": None}


def main():
    # 1. rule chain
    chains = list(tb.page_all("/api/ruleChains"))
    rc = next((c for c in chains if c["name"] == CHAIN_NAME), None)
    if not rc:
        rc = post("/api/ruleChain", {"name": CHAIN_NAME, "type": "CORE", "root": False, "debugMode": False,
                  "additionalInfo": {"description": "Rama 'estado del gemelo' del Contrato de eventos IncuTwin v0.2 + salida al broker MQTT. Entrada desde la raíz de IncuNest (telemetría, atributos, actividad). Filtra por server attribute incutwin_enabled."}})
        print("creada rule chain", CHAIN_NAME)
    post("/api/ruleChain/metadata", chain_metadata(rc["id"]))
    print("metadata actualizada:", CHAIN_NAME, rc["id"]["id"])

    # 2. enganche en la raíz (solo la raíz que es root=true)
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
        dev = get(f"/api/tenant/devices?deviceName={name}")
        attrs = {"incutwin_enabled": True, "inactivityTimeout": 180000}
        for a in args:
            if a.startswith("--attr="):  # p. ej. --attr=itw_t_parents_min=5
                k, v = a[7:].split("=", 1)
                attrs[k] = int(v) if v.isdigit() else (v == "true") if v in ("true", "false") else v
        post(f"/api/plugins/telemetry/DEVICE/{dev['id']['id']}/SERVER_SCOPE", attrs)
        print("habilitada", dev["name"], dev["id"]["id"], attrs)
    print("OK")


if __name__ == "__main__":
    main()
