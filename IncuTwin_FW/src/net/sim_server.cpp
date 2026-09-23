#include "sim_server.h"

#ifdef SIM_MODE

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app_state.h"
#include "app/scenarios.h"

static WebServer *s_http = nullptr;

/* --------------------------------------------------------------------- demo */

static volatile bool s_demo_run = false;
static volatile uint32_t s_demo_interval_ms = 10000;

/* Los escenarios viven en app/scenarios.h: se comparten con el modo demo
 * del boton BOOT (app/demo_mode.h). */

/* ------------------------------------------------------------- campo a campo */

/* Aplica los campos presentes en el JSON. Llamar SIN el lock cogido. */
static void apply_fields(JsonObjectConst obj) {
    state_lock();
    for (JsonPairConst kv : obj) {
        const char *key = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(key, "online")) {
            g_state.incubator_online = v.as<bool>();
        } else if (!strcmp(key, "linked")) {
            g_state.node_seen = v.as<bool>();
        } else if (!strcmp(key, "thermo")) {
            g_state.thermo = thermo_from_str(v.as<const char *>());
        } else if (!strcmp(key, "photo")) {
            g_state.phototherapy = v.as<bool>();
        } else if (!strcmp(key, "hr")) {
            int hr = v.as<int>();
            g_state.heart_rate = (uint16_t)constrain(hr, 0, 300);
        } else if (!strcmp(key, "skin")) {
            int t = v.as<int>();
            if (t >= 0 && t < 6) g_state.skin_tone = (uint8_t)t;
        } else if (!strcmp(key, "awake")) {
            g_state.awake = v.as<bool>();
        } else if (!strcmp(key, "baby")) {
            g_state.baby_present = v.as<bool>();
        }
        /* "scenario" ya se procesó en el handler; claves desconocidas
         * se ignoran, como hace firebase_stream. */
    }
    g_state.last_update_ms = millis();
    g_state_dirty = true;
    state_unlock();
}

/* ---------------------------------------------------------------- handlers */

static const char *thermo_str(thermo_state_t t) {
    switch (t) {
        case THERMO_HEATING: return "heating";
        case THERMO_STABLE: return "stable";
        case THERMO_ALARM: return "alarm";
        default: return "off";
    }
}

static void handle_state_get(void) {
    StaticJsonDocument<256> doc;
    state_lock();
    doc["online"] = g_state.incubator_online;
    doc["linked"] = g_state.node_seen;
    doc["thermo"] = thermo_str(g_state.thermo);
    doc["photo"] = g_state.phototherapy;
    doc["hr"] = g_state.heart_rate;
    doc["skin"] = g_state.skin_tone;
    doc["awake"] = g_state.awake;
    doc["baby"] = g_state.baby_present;
    state_unlock();
    String out;
    serializeJson(doc, out);
    s_http->send(200, "application/json", out);
}

static void handle_state_post(void) {
    String body = s_http->arg("plain");
    Serial.printf("[sim] POST /state %s t=%lu\n", body.c_str(),
                  (unsigned long)millis());
    if (body.length() == 0 || body.length() > 768) {
        s_http->send(400, "text/plain", "bad body size");
        return;
    }
    /* Buffer JSON: 1024 bytes para evitar fallos de NoMemory
     * cuando el JSON válido se acerca a 768 B (tamaño max del cuerpo). */
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok ||
        !doc.is<JsonObjectConst>()) {
        s_http->send(400, "text/plain", "bad json");
        return;
    }
    JsonObjectConst obj = doc.as<JsonObjectConst>();

    /* primero el escenario (si viene), luego los campos lo refinan */
    const char *scen = obj["scenario"] | (const char *)nullptr;
    if (scen && !scenario_apply(scen)) {
        s_http->send(400, "text/plain", "unknown scenario");
        return;
    }
    apply_fields(obj);
    handle_state_get(); /* responde con el estado resultante */
}

static void handle_demo_post(void) {
    String body = s_http->arg("plain");
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok ||
        !doc["run"].is<bool>()) {
        s_http->send(400, "text/plain", "bad json");
        return;
    }
    int ival = doc["interval_s"] | 10;
    s_demo_interval_ms = (uint32_t)constrain(ival, 2, 120) * 1000UL;
    s_demo_run = doc["run"].as<bool>();
    s_http->send(200, "application/json",
                 s_demo_run ? "{\"run\":true}" : "{\"run\":false}");
}

/* Página única de control. Sin dependencias externas. Estilo del portal. */
static const char SIM_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>IncuTwin SIM</title><style>
body{font-family:sans-serif;background:#F4F3F0;color:#1E3E6E;
max-width:420px;margin:0 auto;padding:16px}
h1{color:#054E92;font-size:1.3em}h2{font-size:1em;margin-top:22px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button{padding:12px;font-size:1em;background:#054E92;color:#fff;
border:0;border-radius:10px}
button.sec{background:#7A8CA5}
label{display:block;margin-top:10px;font-weight:bold;font-size:.9em}
select,input[type=range],input[type=number]{width:100%;padding:8px;
margin-top:4px;font-size:1em;border:2px solid #054E92;border-radius:8px;
box-sizing:border-box}
.row{display:flex;gap:10px;align-items:center;margin-top:8px}
.row input{width:auto}
#msg{margin-top:12px;font-weight:bold}
</style></head><body>
<h1>IncuTwin — Simulación</h1>

<h2>Escenarios</h2>
<div class='grid'>
<button onclick="sc('sleep')">Bebé dormido</button>
<button onclick="sc('awake')">Bebé despierto</button>
<button onclick="sc('heating')">Calentando</button>
<button onclick="sc('alarm')">Alarma</button>
<button onclick="sc('photo')">Fototerapia</button>
<button onclick="sc('empty')">Incubadora vacía</button>
<button onclick="sc('off')">IncuNest apagada</button>
<button onclick="sc('unlinked')">Sin vincular</button>
</div>

<h2>Campo a campo</h2>
<label>Termorregulación</label>
<select id='thermo'>
<option value='off'>off</option><option value='heating'>heating</option>
<option value='stable'>stable</option><option value='alarm'>alarm</option>
</select>
<div class='row'><input type='checkbox' id='photo'><span>Fototerapia</span></div>
<div class='row'><input type='checkbox' id='awake'><span>Despierto</span></div>
<div class='row'><input type='checkbox' id='baby'><span>Bebé dentro</span></div>
<div class='row'><input type='checkbox' id='online'><span>IncuNest en línea</span></div>
<div class='row'><input type='checkbox' id='linked'><span>Vinculada</span></div>
<label>Frecuencia cardiaca: <span id='hrv'></span> bpm</label>
<input type='range' id='hr' min='0' max='220'
 oninput="document.getElementById('hrv').textContent=this.value">
<label>Tono de piel: <span id='skinv'></span></label>
<input type='range' id='skin' min='0' max='5'
 oninput="document.getElementById('skinv').textContent=this.value">
<button style='width:100%;margin-top:14px' onclick='apply()'>Aplicar</button>

<h2>Demo automática</h2>
<label>Intervalo (s)</label>
<input type='number' id='ival' value='10' min='2' max='120'>
<div class='grid' style='margin-top:8px'>
<button onclick='demo(true)'>Start</button>
<button class='sec' onclick='demo(false)'>Stop</button>
</div>
<p id='msg'></p>

<script>
function msg(t){document.getElementById('msg').textContent=t;}
function post(url,body){return fetch(url,{method:'POST',
 body:JSON.stringify(body)}).then(r=>{if(!r.ok)throw r.status;return r;});}
function sc(id){post('/state',{scenario:id}).then(load)
 .then(()=>msg('Escenario: '+id)).catch(e=>msg('Error '+e));}
function apply(){
 var b={thermo:document.getElementById('thermo').value,
  photo:document.getElementById('photo').checked,
  awake:document.getElementById('awake').checked,
  baby:document.getElementById('baby').checked,
  online:document.getElementById('online').checked,
  linked:document.getElementById('linked').checked,
  hr:+document.getElementById('hr').value,
  skin:+document.getElementById('skin').value};
 post('/state',b).then(()=>msg('Aplicado')).catch(e=>msg('Error '+e));}
function demo(run){
 post('/demo',{run:run,interval_s:+document.getElementById('ival').value})
 .then(()=>msg(run?'Demo en marcha':'Demo parada')).catch(e=>msg('Error '+e));}
function load(){return fetch('/state').then(r=>r.json()).then(s=>{
 document.getElementById('thermo').value=s.thermo;
 document.getElementById('photo').checked=s.photo;
 document.getElementById('awake').checked=s.awake;
 document.getElementById('baby').checked=s.baby;
 document.getElementById('online').checked=s.online;
 document.getElementById('linked').checked=s.linked;
 document.getElementById('hr').value=s.hr;
 document.getElementById('hrv').textContent=s.hr;
 document.getElementById('skin').value=s.skin;
 document.getElementById('skinv').textContent=s.skin;});}
load();
</script></body></html>)HTML";

static void handle_root(void) {
    s_http->send_P(200, "text/html", SIM_PAGE);
}

/* --------------------------------------------------------------------- task */

static void sim_task(void *arg) {
    (void)arg;
    uint32_t last_touch = 0;
    uint32_t last_demo = 0;
    size_t demo_idx = 0;

    while (true) {
        if (s_http) s_http->handleClient();

        /* Refresca last_update_ms cada segundo para que el watchdog
         * DATA_STALE_S no marque la incubadora como desconectada. */
        if (millis() - last_touch >= 1000) {
            last_touch = millis();
            state_lock();
            g_state.last_update_ms = millis();
            state_unlock();
        }

        /* Demo: rota los escenarios en bucle, en firmware, para
         * sobrevivir al cierre de la pestaña del navegador. */
        if (s_demo_run && millis() - last_demo >= s_demo_interval_ms) {
            last_demo = millis();
            scenario_apply_idx(demo_idx);
            demo_idx = (demo_idx + 1) % scenario_count();
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ---------------------------------------------------------------------- API */

void sim_server_start(void) {
    /* Estado inicial: sin vincular, como un dispositivo recién arrancado. */
    state_lock();
    g_state.cloud_connected = true;
    g_state_dirty = true;
    state_unlock();
    scenario_apply("unlinked");

    s_http = new WebServer(80);
    s_http->on("/", handle_root);
    s_http->on("/state", HTTP_GET, handle_state_get);
    s_http->on("/state", HTTP_POST, handle_state_post);
    s_http->on("/demo", HTTP_POST, handle_demo_post);
    s_http->begin();

    xTaskCreatePinnedToCore(sim_task, "sim_http", 8192, nullptr, 1, nullptr, 0);
    Serial.println("[SIM] servidor de simulacion en puerto 80");
}

#else /* !SIM_MODE */

void sim_server_start(void) {}

#endif
