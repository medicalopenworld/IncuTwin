"""Cliente MQTT de las herramientas (paho-mqtt) hacia mqtt.medicalopenworld.org:8883.

Sustituye a mosquitto_ctrl / mosquitto_pub, que no vienen en Windows por defecto:
  - dynsec(): comandos del plugin dynamic-security via $CONTROL/dynamic-security/v1 (usuario admin)
  - publish(): publica con el rol publisher (QoS 1, retain opcional)
  - subscribe_once(): espera el primer mensaje de un topic (para comprobar status / state)

Credenciales en tools/factory/secrets/broker.env. Nunca se imprimen.
    pip install paho-mqtt
"""
import json
import os
import ssl
import sys
import threading
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:  # pragma: no cover
    sys.exit("pip install paho-mqtt")

HERE = os.path.dirname(os.path.abspath(__file__))
SECRETS_DIR = os.path.join(HERE, "factory", "secrets")
BROKER_ENV = os.path.join(SECRETS_DIR, "broker.env")
CONTROL_TOPIC = "$CONTROL/dynamic-security/v1"
CONTROL_RESPONSE = CONTROL_TOPIC + "/response"


# Nombres que usa infra (MOW) en su .env -> nombres de nuestros scripts. Asi broker.env admite
# las lineas de infra pegadas tal cual (rotacion del 4-oct-2026) sin renombrarlas.
ENV_ALIASES = {
    "MOW_MOSQUITTO_ADMIN_PASSWORD": "MQTT_ADMIN_PASS",
    "MOW_FW_UPLOAD_USER": "FW_UPLOAD_USER",
    "MOW_FW_UPLOAD_PASSWORD": "FW_UPLOAD_PASS",
    "MOW_TBGW_ACCESS_TOKEN": "TBGW_ACCESS_TOKEN",
}


def load_env(path=BROKER_ENV):
    env = {}
    if not os.path.exists(path):
        sys.exit(f"falta {path}")
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            k, v = k.strip(), v.strip().strip('"').strip("'")
            if k.startswith("export "):
                k = k[7:].strip()
            env[k] = v
    for src, dst in ENV_ALIASES.items():
        if env.get(src):  # el nombre de infra manda: es el que rota
            env[dst] = env[src]
    return env


def client_id_for(sn_or_client):
    """ITW-2640-0007 | 2640-0007 | incutwin-2640-0007 -> incutwin-2640-0007."""
    s = sn_or_client.strip()
    if s.startswith("incutwin-"):
        return s
    if s.upper().startswith("ITW-"):
        s = s[4:]
    return f"incutwin-{s}"


class Broker:
    def __init__(self, env, user, password, client_id=None, cafile=None):
        self.env = env
        self.user = user
        self.client_id = client_id or user
        self._c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=self.client_id, protocol=mqtt.MQTTv311)
        self._c.username_pw_set(user, password)
        ctx = ssl.create_default_context(cafile=cafile) if cafile else ssl.create_default_context()
        self._c.tls_set_context(ctx)
        self._connected = threading.Event()
        self._rc = None
        self._c.on_connect = self._on_connect
        self._msgs = []
        self._got = threading.Event()
        self._c.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        self._rc = reason_code
        self._connected.set()

    def _on_message(self, client, userdata, msg):
        self._msgs.append(msg)
        self._got.set()

    def __enter__(self):
        self._c.connect(self.env.get("MQTT_HOST", "mqtt.medicalopenworld.org"), int(self.env.get("MQTT_PORT", "8883")), 30)
        self._c.loop_start()
        if not self._connected.wait(15) or (self._rc is not None and self._rc.is_failure):
            self._c.loop_stop()
            sys.exit(f"no se pudo conectar como {self.user}: {self._rc}")
        return self

    def __exit__(self, *exc):
        self._c.loop_stop()
        self._c.disconnect()

    def publish(self, topic, payload, retain=False, qos=1):
        info = self._c.publish(topic, payload, qos=qos, retain=retain)
        info.wait_for_publish(10)
        if not info.is_published():
            sys.exit(f"no se pudo publicar en {topic}")

    def subscribe_once(self, topic, timeout=10):
        self._msgs.clear()
        self._got.clear()
        self._c.subscribe(topic, qos=1)
        if not self._got.wait(timeout):
            return None
        return self._msgs[0]

    def dynsec(self, commands):
        """Ejecuta una lista de comandos dynamic-security y devuelve sus respuestas.
        Reintenta una vez si el plugin devuelve "Internal error" (pasa cuando esta
        desconectando a un cliente al que acabamos de cambiarle los roles)."""
        for attempt in range(2):
            self._msgs.clear()
            self._got.clear()
            self._c.subscribe(CONTROL_RESPONSE, qos=1)
            time.sleep(0.3)
            self.publish(CONTROL_TOPIC, json.dumps({"commands": commands}))
            if not self._got.wait(10):
                sys.exit("dynsec: sin respuesta del broker")
            resp = json.loads(self._msgs[0].payload.decode()).get("responses", [])
            if attempt == 0 and any("Internal error" in (r.get("error") or "") for r in resp):
                time.sleep(1.5)
                continue
            return resp
        return resp


def admin(env, cafile=None):
    pwd = env.get("MQTT_ADMIN_PASS", "")
    if not pwd:
        sys.exit("MQTT_ADMIN_PASS vacio en broker.env")
    return Broker(env, env.get("MQTT_ADMIN_USER", "admin"), pwd, client_id="incutwin-tools-admin", cafile=cafile)


def publisher(env, cafile=None):
    pwd = env.get("MQTT_PUB_PASS", "")
    if not pwd or pwd == "RELLENAR":
        sys.exit("MQTT_PUB_PASS sin rellenar en broker.env")
    return Broker(env, env.get("MQTT_PUB_USER", "publisher"), pwd, cafile=cafile)


def client_roles(b, username):
    """Roles que ya tiene un cliente (set), o None si no existe."""
    r = b.dynsec([{"command": "getClient", "username": username}])[0]
    if r.get("error"):
        return None
    return {x["rolename"] for x in r.get("data", {}).get("client", {}).get("roles", [])}


def ensure_client_role(b, username, rolename):
    """addClientRole idempotente: dynsec devuelve "Internal error" si ya lo tiene."""
    have = client_roles(b, username) or set()
    if rolename in have:
        return False
    dynsec_ok(b.dynsec([{"command": "addClientRole", "username": username, "rolename": rolename}]))
    return True


def dynsec_ok(responses, allow_errors=()):
    """Lanza si alguna respuesta trae error no tolerado; devuelve la lista."""
    for r in responses:
        err = r.get("error")
        if err and not any(a in err for a in allow_errors):
            sys.exit(f"dynsec {r.get('command')}: {err}")
    return responses
