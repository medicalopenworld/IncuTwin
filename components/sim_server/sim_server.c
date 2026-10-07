#include "sim_server.h"

#include "sdkconfig.h"

#if CONFIG_INCUTWIN_SIM

#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "scenarios.h"
#include "twin_model.h"

static const char *TAG = "sim";

#define BODY_MAX 768

static httpd_handle_t s_httpd;
static twin_incubator_t s_inc;  /* estado "campo a campo" actual */
static bool s_linked;
static esp_timer_handle_t s_demo_timer;
static size_t s_demo_idx;

static const char PAGE[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'><title>IncuTwin SIM</title><style>"
    "body{font-family:sans-serif;background:#F4F3F0;color:#1E3E6E;max-width:420px;margin:0 auto;padding:16px}"
    "h1{color:#054E92;font-size:1.3em}h2{font-size:1em;margin-top:22px}"
    ".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
    "button{padding:12px;font-size:1em;background:#054E92;color:#fff;border:0;border-radius:10px}"
    "button.sec{background:#7A8CA5}label{display:block;margin-top:10px;font-weight:bold;font-size:.9em}"
    "select,input[type=range],input[type=number],textarea{width:100%;padding:8px;margin-top:4px;font-size:1em;"
    "border:2px solid #054E92;border-radius:8px;box-sizing:border-box}"
    ".row{display:flex;gap:10px;align-items:center;margin-top:8px}.row input{width:auto}#msg{margin-top:12px;font-weight:bold}"
    "</style></head><body><h1>IncuTwin — Simulación</h1>"
    "<h2>Escenarios</h2><div class='grid'>"
    "<button onclick=\"sc('sleep')\">Bebé dormido</button><button onclick=\"sc('awake')\">Bebé despierto</button>"
    "<button onclick=\"sc('heating')\">Calentando</button><button onclick=\"sc('alarm')\">Alarma</button>"
    "<button onclick=\"sc('photo')\">Fototerapia</button><button onclick=\"sc('parents')\">Con sus papás</button>"
    "<button onclick=\"sc('home')\">Ya está en casa</button><button onclick=\"sc('empty')\">Incubadora vacía</button>"
    "<button onclick=\"sc('off')\">IncuNest apagada</button><button onclick=\"sc('unlinked')\">Sin vincular</button></div>"
    "<h2>Campo a campo</h2><label>Termorregulación</label><select id='thermo'>"
    "<option value='off'>off</option><option value='heating'>heating</option><option value='stable'>stable</option>"
    "<option value='alarm'>alarm</option></select>"
    "<label>Bebé</label><select id='baby'><option value='none'>none</option><option value='in'>in</option>"
    "<option value='parents'>parents</option><option value='home'>home</option></select>"
    "<div class='row'><input type='checkbox' id='photo'><span>Fototerapia</span></div>"
    "<div class='row'><input type='checkbox' id='awake'><span>Despierto</span></div>"
    "<div class='row'><input type='checkbox' id='online' checked><span>IncuNest en línea</span></div>"
    "<div class='row'><input type='checkbox' id='linked' checked><span>Vinculada</span></div>"
    "<label>Frecuencia cardiaca: <span id='hrv'></span> lpm</label>"
    "<input type='range' id='hr' min='0' max='220' oninput=\"document.getElementById('hrv').textContent=this.value\">"
    "<label>Tono de piel: <span id='skinv'></span></label>"
    "<input type='range' id='skin' min='0' max='5' oninput=\"document.getElementById('skinv').textContent=this.value\">"
    "<label>Nombre (vacío = no compartido)</label><input type='text' id='name' maxlength='23' style='width:100%;padding:8px'>"
    "<button style='width:100%;margin-top:14px' onclick='apply()'>Aplicar</button>"
    "<h2>Payload real (incubators/SIM/state)</h2>"
    "<textarea id='inc' rows='5'>{\"incubator_id\":\"SIM\",\"state\":\"baby\",\"treatments\":[\"heat\",\"pulseox\"],"
    "\"bpm\":138,\"event_seq\":1,\"last_event\":\"baby_in\"}</textarea>"
    "<button style='width:100%;margin-top:8px' onclick='inc()'>Enviar al parser</button>"
    "<h2>Demo automática</h2><label>Intervalo (s)</label><input type='number' id='ival' value='10' min='2' max='120'>"
    "<div class='grid' style='margin-top:8px'><button onclick='demo(true)'>Start</button>"
    "<button class='sec' onclick='demo(false)'>Stop</button></div><p id='msg'></p>"
    "<script>"
    "function msg(t){document.getElementById('msg').textContent=t;}"
    "function post(u,b){return fetch(u,{method:'POST',body:typeof b=='string'?b:JSON.stringify(b)})"
    ".then(r=>{if(!r.ok)return r.text().then(t=>{throw t});return r;});}"
    "function sc(id){post('/state',{scenario:id}).then(load).then(()=>msg('Escenario: '+id)).catch(e=>msg('Error '+e));}"
    "function g(i){return document.getElementById(i);}"
    "function apply(){post('/state',{thermo:g('thermo').value,baby:g('baby').value,photo:g('photo').checked,"
    "awake:g('awake').checked,online:g('online').checked,linked:g('linked').checked,hr:+g('hr').value,"
    "skin:+g('skin').value,name:g('name').value}).then(()=>msg('Aplicado')).catch(e=>msg('Error '+e));}"
    "function inc(){post('/incubator',g('inc').value).then(()=>msg('Payload aplicado')).catch(e=>msg('Error '+e));}"
    "function demo(run){post('/demo',{run:run,interval_s:+g('ival').value})"
    ".then(()=>msg(run?'Demo en marcha':'Demo parada')).catch(e=>msg('Error '+e));}"
    "function load(){return fetch('/state').then(r=>r.json()).then(s=>{g('thermo').value=s.thermo;g('baby').value=s.baby;"
    "g('photo').checked=s.photo;g('awake').checked=s.awake;g('online').checked=s.online;g('linked').checked=s.linked;"
    "g('hr').value=s.hr;g('hrv').textContent=s.hr;g('skin').value=s.skin;g('skinv').textContent=s.skin;g('name').value=s.name;});}"
    "load();</script></body></html>";

/* ------------------------------------------------------------------ helpers */

static const char *thermo_str(twin_thermo_t t)
{
    switch (t) {
    case TWIN_THERMO_HEATING: return "heating";
    case TWIN_THERMO_STABLE:  return "stable";
    case TWIN_THERMO_ALARM:   return "alarm";
    default:                  return "off";
    }
}

static const char *baby_str(twin_baby_t b)
{
    switch (b) {
    case TWIN_BABY_IN:      return "in";
    case TWIN_BABY_PARENTS: return "parents";
    case TWIN_BABY_HOME:    return "home";
    default:                return "none";
    }
}

static bool str_eq(const cJSON *it, const char *s)
{
    return cJSON_IsString(it) && it->valuestring && strcmp(it->valuestring, s) == 0;
}

static void push(void) { twin_model_sim_apply(&s_inc, s_linked); }

static void apply_scenario(const twin_scenario_t *sc)
{
    uint8_t skin = s_inc.skin;
    s_inc = sc->inc;
    s_inc.skin = skin;
    s_linked = sc->linked;
}

static int read_body(httpd_req_t *req, char *buf, size_t max)
{
    int total = req->content_len;
    if (total <= 0 || total > (int)max) return -1;
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0) return -1;
        got += r;
    }
    buf[got] = '\0';
    return got;
}

static esp_err_t send_state(httpd_req_t *req)
{
    char out[320];
    snprintf(out, sizeof(out),
             "{\"online\":%s,\"linked\":%s,\"thermo\":\"%s\",\"baby\":\"%s\",\"photo\":%s,\"hr\":%u,"
             "\"skin\":%u,\"awake\":%s,\"name\":\"%s\",\"weight_g\":%u,\"age_d\":%d}",
             s_inc.online ? "true" : "false", s_linked ? "true" : "false", thermo_str(s_inc.thermo),
             baby_str(s_inc.baby), s_inc.photo ? "true" : "false", s_inc.bpm, s_inc.skin,
             s_inc.awake ? "true" : "false", s_inc.name, s_inc.weight_g, s_inc.age_d);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, out);
}

/* ------------------------------------------------------------------ handlers */

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, PAGE);
}

static esp_err_t state_get(httpd_req_t *req) { return send_state(req); }

static esp_err_t state_post(httpd_req_t *req)
{
    char body[BODY_MAX + 1];
    int n = read_body(req, body, BODY_MAX);
    if (n < 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body size");
    cJSON *root = cJSON_ParseWithLength(body, n);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    const cJSON *sc = cJSON_GetObjectItemCaseSensitive(root, "scenario");
    if (cJSON_IsString(sc)) {
        const twin_scenario_t *s = scenario_find(sc->valuestring);
        if (!s) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown scenario");
        }
        apply_scenario(s);
    }
    const cJSON *it;
    if (cJSON_IsBool(it = cJSON_GetObjectItemCaseSensitive(root, "online"))) s_inc.online = cJSON_IsTrue(it);
    if (cJSON_IsBool(it = cJSON_GetObjectItemCaseSensitive(root, "linked"))) s_linked = cJSON_IsTrue(it);
    if (cJSON_IsBool(it = cJSON_GetObjectItemCaseSensitive(root, "photo"))) s_inc.photo = cJSON_IsTrue(it);
    if (cJSON_IsBool(it = cJSON_GetObjectItemCaseSensitive(root, "awake"))) s_inc.awake = cJSON_IsTrue(it);
    it = cJSON_GetObjectItemCaseSensitive(root, "thermo");
    if (str_eq(it, "off")) s_inc.thermo = TWIN_THERMO_OFF;
    else if (str_eq(it, "heating")) s_inc.thermo = TWIN_THERMO_HEATING;
    else if (str_eq(it, "stable")) s_inc.thermo = TWIN_THERMO_STABLE;
    else if (str_eq(it, "alarm")) s_inc.thermo = TWIN_THERMO_ALARM;
    it = cJSON_GetObjectItemCaseSensitive(root, "baby");
    if (str_eq(it, "none")) s_inc.baby = TWIN_BABY_NONE;
    else if (str_eq(it, "in")) s_inc.baby = TWIN_BABY_IN;
    else if (str_eq(it, "parents")) s_inc.baby = TWIN_BABY_PARENTS;
    else if (str_eq(it, "home")) s_inc.baby = TWIN_BABY_HOME;
    if (cJSON_IsNumber(it = cJSON_GetObjectItemCaseSensitive(root, "hr"))) {
        int v = (int)it->valuedouble;
        s_inc.bpm = (uint16_t)(v < 0 ? 0 : v > TWIN_BPM_MAX ? TWIN_BPM_MAX : v);
        s_inc.pulseox = s_inc.bpm > 0;
    }
    if (cJSON_IsNumber(it = cJSON_GetObjectItemCaseSensitive(root, "skin"))) {
        int v = (int)it->valuedouble;
        if (v >= 0 && v < TWIN_SKIN_COUNT) s_inc.skin = (uint8_t)v;
    }
    it = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (cJSON_IsString(it) && it->valuestring && strlen(it->valuestring) < TWIN_NAME_LEN) {
        strcpy(s_inc.name, it->valuestring);
    }
    if (cJSON_IsNumber(it = cJSON_GetObjectItemCaseSensitive(root, "weight_g"))) s_inc.weight_g = (uint16_t)it->valuedouble;
    if (cJSON_IsNumber(it = cJSON_GetObjectItemCaseSensitive(root, "age_d"))) s_inc.age_d = (int16_t)it->valuedouble;
    cJSON_Delete(root);
    push();
    return send_state(req);
}

static esp_err_t incubator_post(httpd_req_t *req)
{
    char body[BODY_MAX + 1];
    int n = read_body(req, body, BODY_MAX);
    if (n < 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body size");
    twin_model_ingest(body, (size_t)n); /* parser real: valida incubator_id "SIM" */
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static void demo_timer_cb(void *arg)
{
    (void)arg;
    const twin_scenario_t *sc = scenario_get(s_demo_idx);
    if (sc) {
        apply_scenario(sc);
        push();
    }
    s_demo_idx = (s_demo_idx + 1) % scenario_count();
}

static esp_err_t demo_post(httpd_req_t *req)
{
    char body[128];
    int n = read_body(req, body, sizeof(body) - 1);
    if (n < 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body size");
    cJSON *root = cJSON_ParseWithLength(body, n);
    const cJSON *run = root ? cJSON_GetObjectItemCaseSensitive(root, "run") : NULL;
    if (!cJSON_IsBool(run)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    const cJSON *iv = cJSON_GetObjectItemCaseSensitive(root, "interval_s");
    int interval = cJSON_IsNumber(iv) ? (int)iv->valuedouble : 10;
    interval = interval < 2 ? 2 : interval > 120 ? 120 : interval;
    bool on = cJSON_IsTrue(run);
    cJSON_Delete(root);
    esp_timer_stop(s_demo_timer);
    if (on) {
        s_demo_idx = 0;
        esp_timer_start_periodic(s_demo_timer, (uint64_t)interval * 1000000ULL);
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, on ? "{\"run\":true}" : "{\"run\":false}");
}

/* --------------------------------------------------------------------- start */

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const app_evt_wifi_t *ev = data;
    if (ev->connected) ESP_LOGI(TAG, "panel de control: http://%s/", ev->ip);
}

void sim_server_start(void)
{
    twin_model_sim_link(true); /* enlace fingido: credenciales, broker y emparejado "SIM" */
    const twin_scenario_t *sc = scenario_find("unlinked");
    if (sc) apply_scenario(sc);
    s_inc.skin = CONFIG_INCUTWIN_DEFAULT_SKIN;
    push();

    const esp_timer_create_args_t t = { .callback = demo_timer_cb, .name = "sim_demo" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_demo_timer));

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.core_id = 0;
    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo arrancar el servidor");
        return;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = root_get },
        { .uri = "/state", .method = HTTP_GET, .handler = state_get },
        { .uri = "/state", .method = HTTP_POST, .handler = state_post },
        { .uri = "/incubator", .method = HTTP_POST, .handler = incubator_post },
        { .uri = "/demo", .method = HTTP_POST, .handler = demo_post },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) httpd_register_uri_handler(s_httpd, &uris[i]);
    app_events_subscribe(TWIN_EVT_WIFI, on_wifi, NULL);
    ESP_LOGW(TAG, "BUILD DE SIMULACION: servidor web en el puerto 80, sin MQTT ni OTA");
}

#else /* !CONFIG_INCUTWIN_SIM */

void sim_server_start(void) {}

#endif
