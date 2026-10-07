#!/usr/bin/env python3
"""Crea una vez los roles y clientes del contrato en el broker (docs/BROKER-MQTT-CONTEXT.md §4 y §10).
Idempotente: los roles que ya existen se reconcilian (se anaden las ACL que falten, no se quita
ninguna); los clientes que ya existen se dejan como estan.

  python tools/broker_bootstrap.py [--cafile ca.pem]

Roles:
  incutwin    publishClientSend incutwin/%c/# · subscribePattern incutwin/%c/cmd/# e incutwin/all/cmd/#
              (%c = client id; requiere Mosquitto >= 2.1.2, en la 2.0.22 no funcionaba)
  thingsboard publishClientSend incubators/#          (cliente: lo crea infra con su contrasena)
  publisher   publishClientSend incutwin/+/cmd/# · subscribePattern incutwin/+/status e incubators/#
Clientes:
  publisher   si MQTT_PUB_PASS esta vacio/RELLENAR en broker.env se genera una contrasena y se
              escribe ahi (el fichero es local y esta en .gitignore).
Necesita MQTT_ADMIN_PASS en tools/factory/secrets/broker.env.
"""
import argparse
import os
import re
import secrets
import string
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import BROKER_ENV, admin, dynsec_ok, load_env  # noqa: E402

# %c se sustituye por el client id (= usuario en todos los paneles). En Mosquitto 2.0.22 la
# sustitucion no funcionaba en dynamic-security (bug; SUBACK con error y publicaciones
# descartadas) y cada panel llevaba un rol literal panel-<client_id>. Desde la 2.1.2 (broker
# actualizado el 4-oct-2026) basta con el rol incutwin.
ROLES = {
    "incutwin": [
        ("publishClientSend", "incutwin/%c/#"),
        ("subscribePattern", "incutwin/%c/cmd/#"),
        ("subscribePattern", "incutwin/all/cmd/#"),
    ],
    "thingsboard": [
        ("publishClientSend", "incubators/#"),
    ],
    "publisher": [
        ("publishClientSend", "incutwin/+/cmd/#"),
        ("subscribePattern", "incutwin/+/status"),
        ("subscribePattern", "incubators/#"),
        ("publishClientSend", "incubators/#"),  # estados de prueba sin ThingsBoard (§7)
    ],
}


def acl_cmd(role, acltype, topic):
    return {"command": "addRoleACL", "rolename": role, "acltype": acltype, "topic": topic, "allow": True}


def role_acls(b, role):
    """ACL actuales de un rol como set de (acltype, topic)."""
    r = dynsec_ok(b.dynsec([{"command": "getRole", "rolename": role}]))[0]
    acls = r.get("data", {}).get("role", {}).get("acls", [])
    return {(a.get("acltype"), a.get("topic")) for a in acls}


def gen_password(n=24):
    alphabet = string.ascii_letters + string.digits
    return "".join(secrets.choice(alphabet) for _ in range(n))


def write_env_value(key, value):
    with open(BROKER_ENV, encoding="utf-8") as f:
        text = f.read()
    if re.search(rf"^{key}=.*$", text, flags=re.M):
        text = re.sub(rf"^{key}=.*$", f"{key}={value}", text, flags=re.M)
    else:
        text += f"\n{key}={value}\n"
    with open(BROKER_ENV, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cafile")
    args = ap.parse_args()
    env = load_env()

    with admin(env, args.cafile) as b:
        existing = set(b.dynsec([{"command": "listRoles"}])[0].get("data", {}).get("roles", []))
        for role, acls in ROLES.items():
            if role in existing:
                have = role_acls(b, role)
                missing = [(t, topic) for t, topic in acls if (t, topic) not in have]
                if not missing:
                    print(f"rol {role}: ya existe con todas las ACL")
                    continue
                dynsec_ok(b.dynsec([acl_cmd(role, t, topic) for t, topic in missing]))
                print(f"rol {role}: ya existia; {len(missing)} ACL anadidas: " +
                      ", ".join(f"{t} {topic}" for t, topic in missing))
                continue
            cmds = [{"command": "createRole", "rolename": role}]
            cmds += [acl_cmd(role, t, topic) for t, topic in acls]
            dynsec_ok(b.dynsec(cmds))
            print(f"rol {role}: creado con {len(acls)} ACL")

        clients = set(b.dynsec([{"command": "listClients"}])[0].get("data", {}).get("clients", []))
        # clientes de servicio: (usuario, rol, clave de broker.env con su contrasena)
        for user, role, env_key in (
            (env.get("MQTT_PUB_USER", "publisher"), "publisher", "MQTT_PUB_PASS"),
            (env.get("MQTT_TB_USER", "thingsboard"), "thingsboard", "MQTT_TB_PASS"),
        ):
            pwd = env.get(env_key, "")
            if user in clients:
                print(f"cliente {user}: ya existe (contrasena de broker.env sin cambios)")
                if not pwd or pwd == "RELLENAR":
                    print(f"  AVISO: {env_key} sigue sin rellenar; pon la contrasena real o borra el cliente")
                continue
            if not pwd or pwd == "RELLENAR":
                pwd = gen_password()
                write_env_value(env_key, pwd)
                print(f"cliente {user}: contrasena generada y guardada en broker.env ({env_key})")
            dynsec_ok(b.dynsec([
                {"command": "createClient", "username": user, "password": pwd},
                {"command": "addClientRole", "username": user, "rolename": role},
            ]))
            print(f"cliente {user}: creado con rol {role}")
    print("listo")


if __name__ == "__main__":
    main()
