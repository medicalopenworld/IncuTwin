#!/usr/bin/env python3
"""Simulador de una IncuNest para desarrollar contra ThingsBoard (tenant de pruebas).

Manda por HTTP la misma telemetría y los mismos client attributes que una IncuNest real, con el
token de un device simulado (IncuNest-SIM01..03). ThingsBoard pasa esos datos por la rule chain
real de IncuTwin, que decide el estado y lo publica en incubators/<SN>/state. Solo Python 3.8+,
sin dependencias.

  python tools/incunest_sim.py SIM01 run demo        # historia completa (unos 25 min)
  python tools/incunest_sim.py SIM01 run ingreso     # un escenario y sigue mandando hasta Ctrl+C
  python tools/incunest_sim.py SIM01 shell           # consola: escribe escenarios o "set k=v"
  python tools/incunest_sim.py --list                # escenarios disponibles

Token: --token <token>, o el fichero de tokens (SIM_TOKENS_FILE, por defecto
tools/factory/secrets/sim-devices.env con líneas SIM01=<token>). URL: --url o TB_URL.

Tiempos que pone la rule chain (no el simulador): un cambio de tratamiento se publica si han
pasado 30 s desde el anterior; "calor estable" llega tras 5 min a ±0,5 °C del objetivo; la
incubadora pasa a "apagada" tras 2 min sin telemetría; "con sus papás" caduca a los 10 min y una
estancia sin cuidados se cierra a los 15 min (valores del tenant de pruebas).
"""
import argparse
import json
import os
import random
import sys
import threading
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_URL = "https://mon.medicalopenworld.org"
MIN_GAP = 2.5  # segundos mínimos entre dos envíos
TOKENS_FILE = os.environ.get("SIM_TOKENS_FILE") or os.path.join(HERE, "factory", "secrets", "sim-devices.env")


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


class IncuNest:
    """Estado de la IncuNest simulada y envío periódico a ThingsBoard."""

    def __init__(self, url, token, sn, period):
        self.url, self.token, self.sn, self.period = url.rstrip("/"), token, sn, period
        self.lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.last_post = 0.0
        self.paused = False
        self.t = {"SN": sn, "Control_active": False, "Phototherapy_active": False, "Control_mode": "AIR",
                  "Temp_desired": 36.5, "Air_temp": 28.0, "Skin_temp": 34.0,
                  "HR1": 0, "HR1_SQI": 0.0, "SpO2_SQI": 0.0}
        self.pulse = False
        self.baby_seq = self._read_baby_seq()
        self.hand_hold_until = self.read_hand_hold()  # el valor anterior no cuenta como nuevo

    # --- HTTP (API de dispositivo de ThingsBoard: solo necesita el token del device) ---
    def _post(self, kind, body):
        # ThingsBoard procesa en paralelo los mensajes de un mismo device: dos envíos casi a la vez
        # leen el mismo estado previo y el segundo pisa al primero. Se espacian al menos MIN_GAP s.
        with self.send_lock:
            gap = self.last_post + MIN_GAP - time.time()
            if gap > 0:
                time.sleep(gap)
            self.last_post = time.time()
        req = urllib.request.Request(f"{self.url}/api/v1/{self.token}/{kind}", data=json.dumps(body).encode(),
                                     method="POST", headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=15):
                return True
        except (urllib.error.URLError, OSError) as e:
            log("error enviando", kind, getattr(e, "code", ""), getattr(e, "reason", e))
            return False

    def _read_baby_seq(self):
        try:
            with urllib.request.urlopen(f"{self.url}/api/v1/{self.token}/attributes?clientKeys=baby_seq", timeout=15) as r:
                return int(json.loads(r.read() or b"{}").get("client", {}).get("baby_seq", 0))
        except (urllib.error.URLError, OSError, ValueError) as e:
            if getattr(e, "code", None) == 401:
                sys.exit("token no válido para este ThingsBoard")
            return 0

    def attrs(self, values):
        log("atributos", values)
        self._post("attributes", values)

    def event(self, values):
        log("evento", values)
        with self.lock:
            body = dict(self.t, **values)
        self._post("telemetry", body)

    def set(self, **kv):
        with self.lock:
            self.t.update(kv)
        log("set", kv)

    def send_now(self):
        with self.lock:
            t = dict(self.t)
            # la temperatura medida se acerca al objetivo con el control activo (calor "estable" a los 5 min)
            target = t["Temp_desired"] if t["Control_active"] else 26.0
            key = "Skin_temp" if t["Control_mode"] == "SKIN" else "Air_temp"
            self.t[key] = round(self.t[key] + max(-0.6, min(0.6, target - self.t[key])), 1)
            t[key] = self.t[key]
            if self.pulse:
                t["HR1"] = self.t["HR1"] = random.randint(135, 150)
        self._post("telemetry", t)

    def read_hand_hold(self):
        try:
            with urllib.request.urlopen(f"{self.url}/api/v1/{self.token}/attributes?sharedKeys=hand_hold_until",
                                        timeout=15) as r:
                return json.loads(r.read() or b"{}").get("shared", {}).get("hand_hold_until", 0)
        except (urllib.error.URLError, OSError, ValueError):
            return None

    def check_hand_hold(self):
        """Avisa cuando llega un "Coge mi mano" (shared attribute hand_hold_until, en ms)."""
        until = self.read_hand_hold()
        if until and until != self.hand_hold_until:
            log("Coge mi mano recibido: hasta", time.strftime("%H:%M:%S", time.localtime(until / 1000)))
        if until is not None:
            self.hand_hold_until = until

    def loop(self, stop):
        i = 0
        while not stop.is_set():
            if not self.paused:
                self.send_now()
                if i % 3 == 0:
                    self.check_hand_hold()
            i += 1
            stop.wait(self.period)

    # --- acciones de alto nivel ---
    def admit(self, registered=True):
        if registered:
            self.baby_seq += 1
            self.attrs({"baby_seq": self.baby_seq, "baby_admission_epoch": int(time.time()),
                        "baby_kangaroo_count": 0, "baby_thermo_min": 0, "baby_phototherapy_min": 0})
        self.set(Control_active=True, Temp_desired=36.5)

    def pulse_on(self, on=True):
        self.pulse = on
        self.set(HR1_SQI=0.9 if on else 0.0, SpO2_SQI=0.9 if on else 0.0, HR1=140 if on else 0)


def wait(s):
    log(f"espera {s} s")
    time.sleep(s)


def sc_vacia(n):
    n.pulse_on(False)
    n.set(Control_active=False, Phototherapy_active=False)


def sc_ingreso(n):
    n.admit(True)


def sc_ingreso_anonimo(n):
    n.admit(False)


def sc_fototerapia(n):
    n.set(Phototherapy_active=True)


def sc_fin_fototerapia(n):
    n.set(Phototherapy_active=False)


def sc_pulso(n):
    n.pulse_on(True)


def sc_fin_pulso(n):
    n.pulse_on(False)


def sc_canguro(n):
    # la IncuNest registra el canguro y deja de calentar mientras el bebé está con sus padres
    n.pulse_on(False)
    n.set(Control_active=False, Phototherapy_active=False)
    n.event({"baby_seq": n.baby_seq, "baby_kangaroo_event": True, "baby_kangaroo_count": 1})


def sc_vuelta(n):
    n.set(Control_active=True)


def sc_alta(n):
    # primero se apagan los cuidados: con calor activo la rule chain reabriría la estancia (baby_back)
    n.pulse_on(False)
    n.set(Control_active=False, Phototherapy_active=False)
    n.event({"baby_seq": n.baby_seq, "baby_outcome": 1, "baby_discharge_epoch": int(time.time()),
             "baby_thermo_min": 60, "baby_phototherapy_min": 20, "baby_kangaroo_count": 1})


def sc_apagar(n):
    n.paused = True
    log("deja de mandar telemetría: 'apagada' en ~2 min (escribe 'encender' para volver)")


def sc_encender(n):
    n.paused = False
    n.send_now()


def sc_alarma(n):
    # alarma térmica: thermo pasa a "alarm" con el bebé dentro
    n.event({"air_temp_high_alarm": True})


def sc_fin_alarma(n):
    n.event({"air_temp_high_alarm": False})


def sc_rebote(n):
    # cambios cada 10 s durante 1 min: la rule chain solo publica un cambio cada 30 s
    for i in range(6):
        n.set(Phototherapy_active=(i % 2 == 0))
        wait(10)


def sc_demo(n):
    steps = [("ingreso", sc_ingreso, 360), ("fototerapia", sc_fototerapia, 60), ("pulso", sc_pulso, 120),
             ("canguro", sc_canguro, 120), ("vuelta", sc_vuelta, 120), ("fin-fototerapia", sc_fin_fototerapia, 60),
             ("apagar", sc_apagar, 180), ("encender", sc_encender, 60), ("alta", sc_alta, 60)]
    for name, fn, pause in steps:
        log("== demo:", name)
        fn(n)
        wait(pause)
    log("== demo terminada: incubadora vacía")


SCENARIOS = {
    "vacia": (sc_vacia, "sin cuidados (incubadora libre)"),
    "ingreso": (sc_ingreso, "bebé registrado + termorregulación -> baby_in"),
    "ingreso-anonimo": (sc_ingreso_anonimo, "termorregulación sin registrar al bebé -> baby_in anónimo"),
    "fototerapia": (sc_fototerapia, "enciende fototerapia -> treatment_changed"),
    "fin-fototerapia": (sc_fin_fototerapia, "apaga fototerapia"),
    "pulso": (sc_pulso, "pulsioxímetro con buena señal -> bpm"),
    "fin-pulso": (sc_fin_pulso, "quita el pulsioxímetro"),
    "canguro": (sc_canguro, "el bebé sale con sus padres -> baby_parents"),
    "vuelta": (sc_vuelta, "vuelve a la incubadora -> baby_back"),
    "alta": (sc_alta, "alta a casa -> baby_out (home)"),
    "apagar": (sc_apagar, "deja de mandar -> incubator_offline a los ~2 min"),
    "encender": (sc_encender, "vuelve a mandar -> incubator_online"),
    "alarma": (sc_alarma, "alarma térmica -> thermo: alarm"),
    "fin-alarma": (sc_fin_alarma, "quita la alarma"),
    "rebote": (sc_rebote, "fototerapia on/off cada 10 s durante 1 min"),
    "demo": (sc_demo, "historia completa, unos 20 min"),
}


def load_token(sn):
    if not os.path.exists(TOKENS_FILE):
        sys.exit(f"falta {TOKENS_FILE} (o pasa --token)")
    with open(TOKENS_FILE, encoding="utf-8") as f:
        for line in f:
            if line.strip().startswith(sn + "="):
                return line.split("=", 1)[1].strip()
    sys.exit(f"{sn} no está en {TOKENS_FILE}")


def shell(n):
    print("Escenarios:", ", ".join(SCENARIOS), "| set clave=valor | estado | salir")
    for line in sys.stdin:
        cmd = line.strip()
        if not cmd:
            continue
        if cmd in ("salir", "exit", "quit"):
            return
        if cmd == "estado":
            print(json.dumps(dict(n.t, baby_seq=n.baby_seq, enviando=not n.paused), ensure_ascii=False))
        elif cmd.startswith("set "):
            for kv in cmd[4:].split():
                k, _, v = kv.partition("=")
                n.set(**{k: json.loads(v) if v not in ("", None) else v})
            n.send_now()
        elif cmd in SCENARIOS:
            SCENARIOS[cmd][0](n)
            n.send_now()
        else:
            print("no conozco", cmd)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sn", nargs="?", help="SIM01, SIM02 o SIM03")
    ap.add_argument("mode", nargs="?", choices=["run", "shell"])
    ap.add_argument("scenario", nargs="?")
    ap.add_argument("--token")
    ap.add_argument("--url", default=os.environ.get("TB_URL", DEFAULT_URL))
    ap.add_argument("--period", type=float, default=5.0, help="segundos entre envíos (la IncuNest real: 5)")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    if a.list or not a.sn:
        for k, (_, d) in SCENARIOS.items():
            print(f"  {k:16} {d}")
        return
    if a.mode == "run" and a.scenario not in SCENARIOS:
        sys.exit(f"escenario desconocido: {a.scenario} (usa --list)")
    n = IncuNest(a.url, a.token or load_token(a.sn), a.sn, a.period)
    log(f"IncuNest {a.sn} -> {a.url} | baby_seq actual {n.baby_seq} | envío cada {a.period} s")
    stop = threading.Event()
    th = threading.Thread(target=n.loop, args=(stop,), daemon=True)
    th.start()
    try:
        if a.mode == "shell":
            shell(n)
        elif a.mode == "run":
            SCENARIOS[a.scenario][0](n)
            n.send_now()
            if a.scenario == "demo":
                return
            log("sigue mandando telemetría; Ctrl+C para parar")
            while True:
                time.sleep(3600)
        else:
            log("mandando telemetría; Ctrl+C para parar")
            while True:
                time.sleep(3600)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()


if __name__ == "__main__":
    main()
