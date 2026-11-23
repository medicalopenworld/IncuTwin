#!/usr/bin/env python3
"""Desempareja una IncuTwin (spec fleet-tools, pairing).

  python tools/unpair.py <SN|client_id> [--incubator-id 353] [--accompany] [--cafile ca.pem] [--no-role] [--no-tb]

--accompany retira tambien el rol incubators-all del modo Acompanar (spec accompany).

Orden importante: PRIMERO el retenido vacio en incutwin/<client_id>/cmd/pair (solo lo recibe quien
este conectado en ese momento) y DESPUES quitar el rol incubator-<id>, porque dynsec desconecta al
cliente al cambiar sus roles. Si el panel estaba apagado, al volver vera rechazada la suscripcion
al estado y se desemparejara solo (spec pairing).

En ThingsBoard (spec tb-stay-history; --no-tb lo salta, sin credenciales avisa y lo salta): la
estancia en curso queda "acompañada hasta hoy" en el historial del panel, se borra la relacion
MirroredBy y se recalculan itw_panels/itw_panel_count de la IncuNest.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import admin, client_id_for, dynsec_ok, load_env, publisher  # noqa: E402
import tb_pairing  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("panel")
    ap.add_argument("--incubator-id", help="rol incubator-<id> a retirar")
    ap.add_argument("--accompany", action="store_true", help="retirar el rol incubators-all")
    ap.add_argument("--cafile")
    ap.add_argument("--no-role", action="store_true")
    ap.add_argument("--no-tb", action="store_true", help="no tocar ThingsBoard")
    args = ap.parse_args()

    env = load_env()
    cid = client_id_for(args.panel)
    with publisher(env, args.cafile) as p:
        p.publish(f"incutwin/{cid}/cmd/pair", b"", retain=True)
    print(f"publicado cmd/pair retenido vacio para {cid}")
    roles = ([f"incubator-{args.incubator_id}"] if args.incubator_id else []) +             (["incubators-all"] if args.accompany else [])
    if roles and not args.no_role:
        time.sleep(2)  # que el panel procese el desemparejado antes de que dynsec lo desconecte
        with admin(env, args.cafile) as b:
            for role in roles:
                dynsec_ok(b.dynsec([{"command": "removeClientRole", "username": cid,
                                     "rolename": role}]), allow_errors=("not",))
                print(f"rol {role} retirado de {cid}")

    tb = None if args.no_tb else tb_pairing.open_tb()
    if tb:
        tb_pairing.unpair(tb, tb_pairing.panel_device(tb, args.panel, cid))


if __name__ == "__main__":
    main()
