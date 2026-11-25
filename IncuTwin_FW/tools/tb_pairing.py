#!/usr/bin/env python3
"""Emparejamiento en ThingsBoard (spec tb-stay-history): relacion IncuNest —MirroredBy→ panel,
itw_panels / itw_panel_count en la IncuNest y peticion de sync del historial del panel
(server attribute itw_history_sync, que la cadena IncuTwin recibe como Attributes Updated).

Lo usan tools/pair.py y tools/unpair.py; no se ejecuta solo. Sin credenciales de ThingsBoard
(tools/factory/secrets/thingsboard.env) open_tb() devuelve None y los scripts hacen solo la
parte del broker.
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

REL = "MirroredBy"


def open_tb():
    try:
        from tb_api import TB
        return TB()
    except SystemExit as e:  # tb_api sale con un mensaje si faltan credenciales
        print(f"aviso: sin ThingsBoard ({e}); solo se toca el broker")
        return None


def find_device(tb, *names):
    """Primer device con nombre exacto de `names`. Busca por texto (tb_api sale con un 404 si se
    pide un deviceName que no existe)."""
    for n in names:
        if not n:
            continue
        page = tb.get(f"/api/tenant/devices?pageSize=20&page=0&textSearch={n}")
        for d in page.get("data", []):
            if d["name"] == n:
                return d
    return None


def panel_device(tb, panel_arg, client_id):
    """El panel puede estar como ITW-<serie> (alta de fabrica) o como el client id (IoT Gateway)."""
    serial = "ITW-" + client_id[len("incutwin-"):]
    dev = find_device(tb, panel_arg, serial, client_id)
    if not dev:
        sys.exit(f"ThingsBoard: no encuentro el panel ({panel_arg} / {serial} / {client_id})")
    return dev


def incunest_device(tb, incubator_id, tb_name=None):
    dev = find_device(tb, tb_name, f"IncuNest-{incubator_id}")
    if not dev:
        sys.exit(f"ThingsBoard: no encuentro la IncuNest {tb_name or 'IncuNest-' + incubator_id} "
                 f"(usa --tb-device con su nombre exacto)")
    return dev


def _id(dev):
    return dev["id"]["id"]


def relations_to(tb, panel):
    rels = tb.get(f"/api/relations/info?toId={_id(panel)}&toType=DEVICE&relationTypeGroup=COMMON") or []
    return [r for r in rels if r.get("type") == REL and r["from"]["entityType"] == "DEVICE"]


def panels_of(tb, inc_id):
    rels = tb.get(f"/api/relations/info?fromId={inc_id}&fromType=DEVICE&relationTypeGroup=COMMON") or []
    return [r for r in rels if r.get("type") == REL and r["to"]["entityType"] == "DEVICE"]


def refresh_counts(tb, inc_id):
    """itw_panels (nombres) e itw_panel_count en la IncuNest; devuelve los nombres."""
    names = sorted(r.get("toName") or "" for r in panels_of(tb, inc_id))
    names = [n for n in names if n]
    tb.post(f"/api/plugins/telemetry/DEVICE/{inc_id}/SERVER_SCOPE",
            {"itw_panels": json.dumps(names), "itw_panel_count": len(names)})
    return names


def request_sync(tb, panel_id, panel_name, kind="sync"):
    tb.post(f"/api/plugins/telemetry/DEVICE/{panel_id}/SERVER_SCOPE",
            {"itw_history_sync": json.dumps({"kind": kind, "panel": panel_name, "t": int(time.time() * 1000)})})


def _unlink(tb, from_id, panel):
    tb.request("DELETE", f"/api/relation?fromId={from_id}&fromType=DEVICE&relationType={REL}"
                         f"&relationTypeGroup=COMMON&toId={_id(panel)}&toType=DEVICE")


def pair(tb, panel, inc):
    """Deja el panel emparejado solo con `inc` y sincroniza los historiales afectados."""
    others = [r for r in relations_to(tb, panel) if r["from"]["id"] != _id(inc)]
    if others:
        request_sync(tb, _id(panel), panel["name"], "unpaired")  # cierra la estancia de la anterior
        time.sleep(2)
        for r in others:
            _unlink(tb, r["from"]["id"], panel)
            refresh_counts(tb, r["from"]["id"])
            print(f"ThingsBoard: {panel['name']} desvinculado de {r.get('fromName')}")
    if not any(r["from"]["id"] == _id(inc) for r in relations_to(tb, panel)):
        tb.post("/api/relation", {"from": {"id": _id(inc), "entityType": "DEVICE"},
                                  "to": {"id": _id(panel), "entityType": "DEVICE"},
                                  "type": REL, "typeGroup": "COMMON"})
        print(f"ThingsBoard: {inc['name']} -{REL}-> {panel['name']}")
    else:
        print(f"ThingsBoard: relacion {inc['name']} -> {panel['name']} ya existia")
    names = refresh_counts(tb, _id(inc))
    # todos los paneles de la IncuNest: a los demas les cambia "familias que acompañan"
    for r in panels_of(tb, _id(inc)):
        request_sync(tb, r["to"]["id"], r.get("toName") or "")
    print(f"ThingsBoard: {inc['name']} tiene {len(names)} panel(es); historial sincronizado")


def unpair(tb, panel):
    rels = relations_to(tb, panel)
    if not rels:
        print(f"ThingsBoard: {panel['name']} no tenía IncuNest vinculada")
        return
    request_sync(tb, _id(panel), panel["name"], "unpaired")
    time.sleep(2)
    for r in rels:
        _unlink(tb, r["from"]["id"], panel)
        refresh_counts(tb, r["from"]["id"])
        for o in panels_of(tb, r["from"]["id"]):
            request_sync(tb, o["to"]["id"], o.get("toName") or "")
        print(f"ThingsBoard: {panel['name']} desvinculado de {r.get('fromName')}")
