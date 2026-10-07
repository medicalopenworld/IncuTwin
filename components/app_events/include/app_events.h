/* IncuTwin — bus de eventos de la aplicacion (loop esp_event propio).
 *
 * Desacopla red -> modelo -> UI/sonido (design.md D4). Los productores publican
 * con app_events_post(); los consumidores se suscriben con app_events_subscribe().
 * Los handlers corren en la tarea "incutwin_evt" (core 0, prio 5): deben ser
 * cortos (< 5 ms) y nunca tocar LVGL sin lvgl_port_lock(); la UI copia el dato y
 * lo aplica en su propio lv_timer.
 *
 * esp_event copia el payload del evento, asi que los structs de datos van por
 * valor y sin punteros a memoria del productor.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"
#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(TWIN_EVENTS);

typedef enum {
    /* twin_model -> ui, sound, usage_stats. data: twin_snapshot_t */
    TWIN_EVT_STATE_CHANGED = 1,
    /* twin_model -> sound. data: twin_transition_t */
    TWIN_EVT_TRANSITION,
    /* net_wifi -> twin_model, cloud_status. data: app_evt_wifi_t */
    TWIN_EVT_WIFI,
    /* mqtt_link -> twin_model, cloud_status, ota_update. data: app_evt_mqtt_t */
    TWIN_EVT_MQTT,
    /* mqtt_link -> commands, twin_model. data: app_evt_mqtt_msg_t */
    TWIN_EVT_MQTT_MSG,
    /* settings -> ui, sound. data: app_evt_settings_t */
    TWIN_EVT_SETTINGS_CHANGED,
    /* pairing -> mqtt_link, cloud_status. data: app_evt_pairing_t */
    TWIN_EVT_PAIRING_CHANGED,
    /* mqtt_link -> pairing: el broker rechazo la suscripcion al estado de la incubadora
     * (ya no tenemos ese rol) -> desemparejar. data: app_evt_pairing_t con el id rechazado */
    TWIN_EVT_STATE_SUB_REJECTED,
} app_event_id_t;

typedef struct {
    bool connected;
    int8_t rssi;
    char ip[16];
} app_evt_wifi_t;

typedef struct {
    bool connected;
} app_evt_mqtt_t;

#define APP_EVT_TOPIC_MAX   96
#define APP_EVT_PAYLOAD_MAX 1024

typedef struct {
    char topic[APP_EVT_TOPIC_MAX];
    uint16_t len;
    char payload[APP_EVT_PAYLOAD_MAX + 1]; /* terminado en 0 */
} app_evt_mqtt_msg_t;

typedef struct {
    uint8_t lang;       /* 0 ES, 1 EN */
    uint8_t volume;     /* 0..3 */
    uint8_t brightness; /* 10..100 */
} app_evt_settings_t;

typedef struct {
    char incubator_id[TWIN_INCUBATOR_ID_LEN]; /* "" = desemparejado */
} app_evt_pairing_t;

esp_err_t app_events_init(void);
esp_err_t app_events_post(app_event_id_t id, const void *data, size_t size);
esp_err_t app_events_subscribe(app_event_id_t id, esp_event_handler_t handler, void *arg);

#ifdef __cplusplus
}
#endif
