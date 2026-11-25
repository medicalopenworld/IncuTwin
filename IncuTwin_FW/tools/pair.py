#!/usr/bin/env python3
"""Empareja una IncuTwin con una incubadora (spec fleet-tools, pairing).

  python tools/pair.py <SN|client_id> <incubator_id> [--cafile ca.pem] [--no-role]
                       [--tb-device IncuNest-353] [--no-tb]
  python tools/pair.py <SN|client_id> accompany      (modo Acompanar, spec accompany)

Con "accompany" en vez de un id: rol incubators-all (subscribePattern incubators/+/state) y
{"mode":"accompany"} retenido; la IncuTwin elige sola la IncuNest con bebe. No toca ThingsBoard.

1. Crea el rol incubator-<id> si no existe (subscribePattern incubators/<id>/state) y se lo da
   al cliente (usuario admin via dynamic-security; con --no-role se salta).
2. Publica retenido, QoS 1, {"incubator_id":"<id>"} en incutwin/<client_id>/cmd/pair con el
   rol publisher.
3. En ThingsBoard (spec tb-stay-history; --no-tb lo salta, sin credenciales avisa y lo salta):
   relacion IncuNest —MirroredBy→ panel (quita la de otra IncuNest), itw_panels/itw_panel_count
   y sync del historial de todos los paneles de esa IncuNest. --tb-device = nombre del device de
   la IncuNest si no es IncuNest-<incubator_id>.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import admin, client_id_for, dynsec_ok, load_env, publisher  # noqa: E402
import tb_pairing  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("panel", help="serie (ITW-2640-0007) o client id (incutwin-2640-0007)")
    ap.add_argument("incubator_id", help="p. ej. 353")
    ap.add_argument("--cafile")
    ap.add_argument("--no-role", action="store_true", help="no tocar roles (sin admin)")
    ap.add_argument("--tb-device", help="nombre del device de la IncuNest en ThingsBoard")
    ap.add_argument("--no-tb", action="store_true", help="no tocar ThingsBoard")
    args = ap.parse_args()

    accompany = args.incubator_id == "accompany"
    if not accompany and not re.fullmatch(r"[A-Za-z0-9_-]{1,16}", args.incubator_id):
        raise SystemExit("incubator_id invalido: 1-16 caracteres [A-Za-z0-9_-]")
    env = load_env()
    cid = client_id_for(args.panel)
    role = "incubators-all" if accompany else f"incubator-{args.incubator_id}"
    topic = "incubators/+/state" if accompany else f"incubators/{args.incubator_id}/state"

    if not args.no_role:
        with admin(env, args.cafile) as b:
            if any(r.get("error") for r in b.dynsec([{"command": "getRole", "rolename": role}])):
                dynsec_ok(b.dynsec([
                    {"command": "createRole", "rolename": role},
                    {"command": "addRoleACL", "rolename": role, "acltype": "subscribePattern",
                     "topic": topic, "allow": True},
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

    payload = {"mode": "accompany"} if accompany else {"incubator_id": args.incubator_id}
    with publisher(env, args.cafile) as p:
        p.publish(f"incutwin/{cid}/cmd/pair", json.dumps(payload), retain=True)
    print(f"publicado cmd/pair retenido para {cid} -> {json.dumps(payload)}")
    if accompany:
        print("modo acompanar: ThingsBoard no se toca (la IncuNest la elige el panel)")
        return

    tb = None if args.no_tb else tb_pairing.open_tb()
    if tb:
        panel = tb_pairing.panel_device(tb, args.panel, cid)
        inc = tb_pairing.incunest_device(tb, args.incubator_id, args.tb_device)
        tb_pairing.pair(tb, panel, inc)


if __name__ == "__main__":
    main()
