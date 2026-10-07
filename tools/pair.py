#!/usr/bin/env python3
"""Empareja una IncuTwin con una incubadora (spec fleet-tools, pairing).

  python tools/pair.py <SN|client_id> <incubator_id> [--cafile ca.pem] [--no-role]

1. Crea el rol incubator-<id> si no existe (subscribePattern incubators/<id>/state) y se lo da
   al cliente (usuario admin via dynamic-security; con --no-role se salta).
2. Publica retenido, QoS 1, {"incubator_id":"<id>"} en incutwin/<client_id>/cmd/pair con el
   rol publisher.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import admin, client_id_for, dynsec_ok, load_env, publisher  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("panel", help="serie (ITW-2640-0007) o client id (incutwin-2640-0007)")
    ap.add_argument("incubator_id", help="p. ej. 353")
    ap.add_argument("--cafile")
    ap.add_argument("--no-role", action="store_true", help="no tocar roles (sin admin)")
    args = ap.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9_-]{1,16}", args.incubator_id):
        raise SystemExit("incubator_id invalido: 1-16 caracteres [A-Za-z0-9_-]")
    env = load_env()
    cid = client_id_for(args.panel)
    role = f"incubator-{args.incubator_id}"

    if not args.no_role:
        with admin(env, args.cafile) as b:
            if any(r.get("error") for r in b.dynsec([{"command": "getRole", "rolename": role}])):
                dynsec_ok(b.dynsec([
                    {"command": "createRole", "rolename": role},
                    {"command": "addRoleACL", "rolename": role, "acltype": "subscribePattern",
                     "topic": f"incubators/{args.incubator_id}/state", "allow": True},
                ]))
                print(f"rol {role} creado")
            # dynsec responde "Internal error" si el cliente ya tiene el rol: comprobar antes
            info = b.dynsec([{"command": "getClient", "username": cid}])[0]
            have = {r["rolename"] for r in info.get("data", {}).get("client", {}).get("roles", [])}
            if role in have:
                print(f"rol {role} ya asignado a {cid}")
            else:
                dynsec_ok(b.dynsec([{"command": "addClientRole", "username": cid, "rolename": role}]))
                print(f"rol {role} asignado a {cid}")

    with publisher(env, args.cafile) as p:
        p.publish(f"incutwin/{cid}/cmd/pair", json.dumps({"incubator_id": args.incubator_id}), retain=True)
    print(f"publicado cmd/pair retenido para {cid} -> {args.incubator_id}")


if __name__ == "__main__":
    main()
