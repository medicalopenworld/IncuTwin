#!/usr/bin/env python3
"""Cliente minimo de la API REST de ThingsBoard para las herramientas de IncuTwin.

Lee tools/factory/secrets/thingsboard.env (TB_URL + TB_API_KEY, o TB_USER/TB_PASS). Nunca
imprime el token. Solo urllib (sin dependencias).

  python tools/tb_api.py --check          # version, tenant y numero de rule chains / devices
  python tools/tb_api.py GET /api/tenant/devices?pageSize=5&page=0
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TB_ENV = os.path.join(HERE, "factory", "secrets", "thingsboard.env")


def load_env(path=TB_ENV):
    """thingsboard.env si existe; si no, las variables de entorno TB_URL/TB_API_KEY/TB_USER/TB_PASS."""
    env = {k: v for k, v in os.environ.items() if k in ("TB_URL", "TB_API_KEY", "TB_USER", "TB_PASS")}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                if v.strip():
                    env[k.strip()] = v.strip()
    if not env.get("TB_URL"):
        sys.exit(f"falta {path} (copia tools/factory/thingsboard.env.example) o la variable TB_URL")
    return env


class TB:
    def __init__(self, env=None):
        self.env = env or load_env()
        self.url = self.env.get("TB_URL", "").rstrip("/")
        if not self.url:
            sys.exit("TB_URL vacio en thingsboard.env")
        # ThingsBoard usa la cabecera X-Authorization: "ApiKey <key>" o "Bearer <jwt>"
        key = self.env.get("TB_API_KEY", "")
        if key and key != "RELLENAR":
            self._auth = f"ApiKey {key}"
        elif self.env.get("TB_USER") and self.env.get("TB_PASS"):
            self._auth = None
            tok = self.request("POST", "/api/auth/login",
                               {"username": self.env["TB_USER"], "password": self.env["TB_PASS"]}, auth=False)
            self._auth = f"Bearer {tok['token']}"
        else:
            sys.exit("rellena TB_API_KEY (o TB_USER/TB_PASS) en tools/factory/secrets/thingsboard.env")

    def request(self, method, path, body=None, auth=True):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(self.url + path, data=data, method=method)
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json")
        if auth:
            req.add_header("X-Authorization", self._auth)
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                raw = r.read()
                return json.loads(raw) if raw else None
        except urllib.error.HTTPError as e:
            msg = e.read().decode(errors="replace")[:400]
            sys.exit(f"TB {method} {path}: HTTP {e.code} {msg}")

    def get(self, path):
        return self.request("GET", path)

    def post(self, path, body):
        return self.request("POST", path, body)

    def delete(self, path):
        return self.request("DELETE", path)

    def page_all(self, path, page_size=100):
        """Itera todos los elementos de un endpoint paginado (?pageSize&page)."""
        page = 0
        sep = "&" if "?" in path else "?"
        while True:
            r = self.get(f"{path}{sep}pageSize={page_size}&page={page}")
            for item in r.get("data", []):
                yield item
            if not r.get("hasNext"):
                return
            page += 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("method", nargs="?")
    ap.add_argument("path", nargs="?")
    args = ap.parse_args()
    tb = TB()
    if args.check:
        info = tb.get("/api/system/info") if False else None  # requiere sysadmin; usamos endpoints de tenant
        user = tb.get("/api/auth/user")
        print("usuario:", user.get("email"), "| authority:", user.get("authority"))
        chains = list(tb.page_all("/api/ruleChains"))
        print("rule chains:", len(chains), [c["name"] for c in chains][:20])
        devs = tb.get("/api/tenant/devices?pageSize=1&page=0")
        print("devices:", devs.get("totalElements"))
        profiles = list(tb.page_all("/api/deviceProfiles"))
        print("device profiles:", [p["name"] for p in profiles])
        return
    if not args.method or not args.path:
        ap.error("--check o METODO RUTA")
    print(json.dumps(tb.request(args.method.upper(), args.path), indent=2, ensure_ascii=False)[:4000])


if __name__ == "__main__":
    main()
