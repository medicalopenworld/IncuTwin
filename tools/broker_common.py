"""Utilidades comunes de las herramientas del broker (pair.py, unpair.py, release.py).

Lee tools/factory/secrets/broker.env (gitignored) y localiza mosquitto_ctrl / mosquitto_pub.
Nunca imprime contrasenas.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FACTORY_DIR = os.path.join(HERE, "factory")
SECRETS_DIR = os.path.join(FACTORY_DIR, "secrets")
BROKER_ENV = os.path.join(SECRETS_DIR, "broker.env")


def load_env(path=BROKER_ENV):
    env = {}
    if not os.path.exists(path):
        sys.exit(f"falta {path} (ver tools/factory/secrets/broker.env de ejemplo)")
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip()
    return env


def client_id_for(sn_or_client):
    """Acepta ITW-2640-0007, 2640-0007 o incutwin-2640-0007."""
    s = sn_or_client.strip()
    if s.startswith("incutwin-"):
        return s
    if s.upper().startswith("ITW-"):
        s = s[4:]
    return f"incutwin-{s}"


def find_tool(name):
    exe = shutil.which(name) or shutil.which(name + ".exe")
    if not exe and os.name == "nt":
        cand = os.path.join(r"C:\Program Files\mosquitto", name + ".exe")
        exe = cand if os.path.exists(cand) else None
    if not exe:
        sys.exit(f"{name} no encontrado: instala Mosquitto (https://mosquitto.org/download/)")
    return exe


def tls_args(env, cafile=None):
    return ["--cafile", cafile] if cafile else ["--tls-use-os-certs"]


def host_args(env):
    return ["-h", env.get("MQTT_HOST", "mqtt.medicalopenworld.org"), "-p", env.get("MQTT_PORT", "8883")]


def ctrl(env, cafile=None):
    """Base de mosquitto_ctrl ... dynsec con el usuario admin."""
    pwd = env.get("MQTT_ADMIN_PASS", "")
    if not pwd:
        sys.exit("MQTT_ADMIN_PASS vacio en broker.env")
    return [find_tool("mosquitto_ctrl")] + host_args(env) + tls_args(env, cafile) + \
           ["-u", env.get("MQTT_ADMIN_USER", "admin"), "-P", pwd, "dynsec"]


def pub(env, topic, payload, retain, cafile=None):
    """mosquitto_pub con el rol publisher, QoS 1. payload None = mensaje vacio (-n)."""
    user = env.get("MQTT_PUB_USER", "publisher")
    pwd = env.get("MQTT_PUB_PASS", "")
    if not pwd or pwd == "RELLENAR":
        sys.exit("MQTT_PUB_PASS sin rellenar en broker.env")
    cmd = [find_tool("mosquitto_pub")] + host_args(env) + tls_args(env, cafile) + \
          ["-u", user, "-P", pwd, "-i", user, "-q", "1", "-t", topic]
    cmd += ["-r"] if retain else []
    cmd += ["-n"] if payload is None else ["-m", payload]
    return run(cmd)


def run(cmd, allow_fail=False):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 and not allow_fail:
        safe = [("***" if (i > 0 and cmd[i - 1] == "-P") else c) for i, c in enumerate(cmd)]
        sys.exit(f"ERROR: {' '.join(safe)}\n{r.stdout}\n{r.stderr}")
    return r
