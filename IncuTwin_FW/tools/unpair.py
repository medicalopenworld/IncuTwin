#!/usr/bin/env python3
"""Desempareja una IncuTwin (spec fleet-tools, pairing).

  python tools/unpair.py <SN|client_id> [--incubator-id 353] [--cafile ca.pem] [--no-role]

Orden importante: PRIMERO el retenido vacio en incutwin/<client_id>/cmd/pair (solo lo recibe quien
este conectado en ese momento) y DESPUES quitar el rol incubator-<id>, porque dynsec desconecta al
cliente al cambiar sus roles. Si el panel estaba apagado, al volver vera rechazada la suscripcion
al estado y se desemparejara solo (spec pairing).
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import admin, client_id_for, dynsec_ok, load_env, publisher  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("panel")
    ap.add_argument("--incubator-id", help="rol incubator-<id> a retirar")
    ap.add_argument("--cafile")
    ap.add_argument("--no-role", action="store_true")
    args = ap.parse_args()

    env = load_env()
    cid = client_id_for(args.panel)
    with publisher(env, args.cafile) as p:
        p.publish(f"incutwin/{cid}/cmd/pair", b"", retain=True)
    print(f"publicado cmd/pair retenido vacio para {cid}")
    if args.incubator_id and not args.no_role:
        time.sleep(2)  # que el panel procese el desemparejado antes de que dynsec lo desconecte
        with admin(env, args.cafile) as b:
            dynsec_ok(b.dynsec([{"command": "removeClientRole", "username": cid,
                                 "rolename": f"incubator-{args.incubator_id}"}]), allow_errors=("not",))
        print(f"rol incubator-{args.incubator_id} retirado de {cid}")


if __name__ == "__main__":
    main()
