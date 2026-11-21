#!/usr/bin/env python3
"""Contornos de tierra para el mapa de la pagina del pais (spec twin-display).

El panel pinta el mapa el mismo (components/world_map), con el zoom que haga falta para que se
vean a la vez tu IncuTwin y la IncuNest; aqui solo se exportan los contornos como datos C.
Origen: Natural Earth 1:110m land (dominio publico), tools/data/ne_110m_land.geojson.
Salida: components/world_map/map_land.c (centesimas de grado en int16, ~20 KB) con el
rango de latitudes de cada anillo para descartar rapido los que no cruzan una fila.
Uso: python tools/gen_map.py"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "components", "world_map", "map_land.c")


def rings(geom):
    polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]
    for poly in polys:
        yield from poly  # exterior y huecos: el relleno par-impar los resuelve


def main():
    with open(os.path.join(HERE, "data", "ne_110m_land.geojson"), encoding="utf-8") as f:
        land = json.load(f)
    pts, lens, lat_min, lat_max = [], [], [], []
    for feat in land["features"]:
        for ring in rings(feat["geometry"]):
            q = [(round(lon * 100), round(lat * 100)) for lon, lat in ring]
            if q[0] == q[-1]:
                q = q[:-1]  # el cierre lo pone el rasterizador
            if len(q) < 3:
                continue
            pts += q
            lens.append(len(q))
            lat_min.append(min(p[1] for p in q))
            lat_max.append(max(p[1] for p in q))
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Contornos de tierra: Natural Earth 1:110m land (dominio publico).\n"
                " * Generado por tools/gen_map.py — NO editar a mano. Centesimas de grado. */\n")
        f.write('#include "map_land.h"\n\n')
        f.write(f"const uint16_t MAP_LAND_RINGS = {len(lens)};\n")
        f.write(f"const uint16_t MAP_LAND_POINTS = {len(pts)};\n\n")
        f.write("const int16_t map_land_pts[][2] = {  /* lon, lat */\n")
        for i in range(0, len(pts), 8):
            f.write("  " + " ".join(f"{{{lo},{la}}}," for lo, la in pts[i:i + 8]) + "\n")
        f.write("};\n\nconst uint16_t map_land_ring_len[] = {\n")
        for i in range(0, len(lens), 16):
            f.write("  " + " ".join(f"{n}," for n in lens[i:i + 16]) + "\n")
        f.write("};\n\nconst int16_t map_land_ring_lat[][2] = {  /* min, max */\n")
        for i in range(0, len(lens), 8):
            f.write("  " + " ".join(f"{{{a},{b}}}," for a, b in zip(lat_min[i:i + 8], lat_max[i:i + 8])) + "\n")
        f.write("};\n")
    print(f"map_land.c: {len(lens)} anillos, {len(pts)} puntos")


if __name__ == "__main__":
    main()
