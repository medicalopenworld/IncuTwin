#!/usr/bin/env python3
"""Clientes del broker para las IncuNest simuladas del tenant de pruebas (tb/tb_apply_sim_tenant.py).

  python tools/broker_sim_clients.py --out-dir <carpeta de secrets del tenant de pruebas>

Roles (solo los topics de las simuladas; nunca incubators/# entero):
  thingsboard-sim   publishClientSend incubators/SIM0x/state   -> nodo MQTT del tenant Demo
  incutwin-api-dev  subscribePattern  incubators/SIM0x/state   -> API de NEXT en desarrollo (solo lectura)
Las contraseñas se generan una vez y se guardan en <out-dir>/broker-sim.env (MQTT_TB_USER/PASS, para
tb_apply_sim_tenant.py --mqtt-env) y <out-dir>/next-broker-dev.env (lo que se entrega a NEXT).
Si el cliente ya existe y su fichero también, no se cambia nada. Nada se imprime.
Necesita MQTT_ADMIN_PASS en tools/factory/secrets/broker.env.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_bootstrap import acl_cmd, gen_password, role_acls  # noqa: E402
from broker_mqtt import admin, dynsec_ok, ensure_client_role, load_env  # noqa: E402

SIMS = ["SIM01", "SIM02", "SIM03"]
CLIENTS = [
    # (usuario/rol, tipo de ACL, fichero, plantilla)
    ("thingsboard-sim", "publishClientSend", "broker-sim.env",
     "# Cliente del broker para el nodo MQTT del tenant de pruebas (solo publica en las SIM)\n"
     "MQTT_TB_USER={user}\nMQTT_TB_PASS={pwd}\n"),
    ("incutwin-api-dev", "subscribePattern", "next-broker-dev.env",
     "# Broker MQTT de IncuTwin - acceso de desarrollo (solo lectura de las IncuNest simuladas)\n"
     "MQTT_URL=mqtts://mqtt.medicalopenworld.org:8883\n"
     "MQTT_USERNAME={user}\nMQTT_CLIENT_ID={user}\nMQTT_PASSWORD={pwd}\n"
     "MQTT_TOPICS=" + ",".join(f"incubators/{s}/state" for s in SIMS) + "\n"),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--cafile")
    a = ap.parse_args()
    env = load_env()
    with admin(env, a.cafile) as b:
        roles = set(b.dynsec([{"command": "listRoles"}])[0].get("data", {}).get("roles", []))
        clients = set(b.dynsec([{"command": "listClients"}])[0].get("data", {}).get("clients", []))
        for user, acltype, fname, tpl in CLIENTS:
            want = [(acltype, f"incubators/{s}/state") for s in SIMS]
            if user not in roles:
                dynsec_ok(b.dynsec([{"command": "createRole", "rolename": user}]))
                have = set()
            else:
                have = role_acls(b, user)
            missing = [x for x in want if x not in have]
            if missing:
                dynsec_ok(b.dynsec([acl_cmd(user, t, topic) for t, topic in missing]))
            extra = have - set(want)
            print(f"rol {user}: {len(want)} ACL ({len(missing)} nuevas)" + (f" | OJO ACL extra: {extra}" if extra else ""))

            path = os.path.join(a.out_dir, fname)
            if user in clients:
                if not os.path.exists(path):
                    sys.exit(f"cliente {user} ya existe pero falta {path}: no sé su contraseña")
                print(f"cliente {user}: ya existe")
            else:
                pwd = gen_password(32)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(tpl.format(user=user, pwd=pwd))
                dynsec_ok(b.dynsec([{"command": "createClient", "username": user, "password": pwd}]))
                print(f"cliente {user}: creado; credenciales en {path}")
            ensure_client_role(b, user, user)
    print("listo")


if __name__ == "__main__":
    main()
