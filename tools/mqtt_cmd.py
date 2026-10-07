#!/usr/bin/env python3
"""Publica un comando a una IncuTwin o escucha sus topics, con el rol publisher.

  python tools/mqtt_cmd.py <SN|client_id|all> <comando> [payload-json]   # p. ej. reboot, brightness '{"value":40}'
  python tools/mqtt_cmd.py --status <SN|client_id> [--timeout 10]        # espera el status retenido
  python tools/mqtt_cmd.py --state <incubator_id> [payload-json]         # publica un estado de prueba (retenido)
  python tools/mqtt_cmd.py --watch <SN|client_id> [--timeout 60]         # imprime status/cmd durante un rato
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from broker_mqtt import client_id_for, load_env, publisher  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", nargs="?")
    ap.add_argument("command", nargs="?")
    ap.add_argument("payload", nargs="?", default="")
    ap.add_argument("--status", metavar="PANEL")
    ap.add_argument("--state", metavar="INCUBATOR_ID")
    ap.add_argument("--watch", metavar="PANEL")
    ap.add_argument("--timeout", type=int, default=10)
    ap.add_argument("--cafile")
    args = ap.parse_args()
    env = load_env()

    with publisher(env, args.cafile) as p:
        if args.status:
            cid = client_id_for(args.status)
            m = p.subscribe_once(f"incutwin/{cid}/status", args.timeout)
            print(f"{cid}: {m.payload.decode() if m else '(sin status retenido)'}")
        elif args.state:
            payload = args.target or ('{"incubator_id":"%s","ts":%d,"state":"baby","treatments":["heat","pulseox"],'
                                      '"bpm":138,"last_seen":%d,"event_seq":%d,"last_event":"baby_in"}'
                                      % (args.state, int(time.time()), int(time.time()), int(time.time()) % 100000))
            p.publish(f"incubators/{args.state}/state", payload, retain=True)
            print(f"publicado incubators/{args.state}/state: {payload}")
        elif args.watch:
            cid = client_id_for(args.watch)
            p._c.subscribe(f"incutwin/{cid}/#", qos=1)
            t0 = time.time()
            seen = 0
            while time.time() - t0 < args.timeout:
                if len(p._msgs) > seen:
                    for m in p._msgs[seen:]:
                        print(f"{time.strftime('%H:%M:%S')} {m.topic}: {m.payload.decode(errors='replace')}")
                    seen = len(p._msgs)
                time.sleep(0.2)
        else:
            if not args.target or not args.command:
                ap.error("hacen falta <target> y <comando>")
            cid = args.target if args.target == "all" else client_id_for(args.target)
            topic = f"incutwin/{cid}/cmd/{args.command}"
            p.publish(topic, args.payload, retain=False)
            print(f"publicado {topic} {args.payload}")


if __name__ == "__main__":
    main()
