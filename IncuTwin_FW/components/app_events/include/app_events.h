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
    /* ui -> cloud_status: gesto "Coge mi mano" completado y hay que avisar a la IncuNest.
     * data: ninguno (el id emparejado lo pone la red) */
    TWIN_EVT_HAND_HOLD,
    /* ui -> accompany: gesto completado o toque en la despedida. data: app_evt_acc_input_t */
    TWIN_EVT_ACC_INPUT,
    /* twin_model -> event_log, ui, sound. data: app_evt_notice_t */
    TWIN_EVT_NOTICE,
    /* event_log -> ui: cambio el historial de avisos o el contador de no vistos. data: ninguno */
    TWIN_EVT_LOG_CHANGED,
    /* stays -> ui: cambiaron los bebes de las IncuNest (Historial). data: ninguno */
    TWIN_EVT_STAYS_CHANGED,
    /* rewards -> ui: cambiaron corazones, sellos, pasaporte o lo elegido. data: app_evt_rewards_t */
    TWIN_EVT_REWARDS_CHANGED,
    /* ota_update -> ui: hay una OTA lista para descargar; la UI abre su ventana y llama a
     * ota_update_go() (spec ota-update). data: ninguno */
    TWIN_EVT_OTA_PENDING,
    /* settings -> cloud_status: cambio el nombre de la IncuTwin (status lo lleva). data: ninguno */
    TWIN_EVT_NAME_CHANGED,
    /* incupray -> ui: cambio el horario, live, summary o la participacion. data: ninguno */
    TWIN_EVT_INCUPRAY,
    /* mqtt_link -> telemetry: el broker confirmo (PUBACK) un publish QoS 1 hecho con
     * mqtt_link_publish_id(). data: app_evt_puback_t */
    TWIN_EVT_MQTT_PUBACK,
} app_event_id_t;

typedef enum {
    APP_ACC_GESTURE = 1, /* "Coge mi mano" completado (aunque no se pueda publicar) */
    APP_ACC_TOUCH,       /* toque en la pantalla durante la despedida */
    APP_ACC_SUPER,       /* 3 minutos de mano en el mismo gesto: super corazon */
} app_evt_acc_input_t;

typedef struct {
    bool connected;
    int8_t rssi;
    char ip[16];
} app_evt_wifi_t;

typedef struct {
    bool connected;
} app_evt_mqtt_t;

typedef struct {
    int msg_id; /* el que devolvio mqtt_link_publish_id() */
} app_evt_puback_t;

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
    uint8_t quiet_on;   /* horario nocturno activo (0/1) */
    uint8_t quiet_from; /* 0..23 */
    uint8_t quiet_to;   /* 0..23 */
} app_evt_settings_t;

typedef struct {
    char incubator_id[TWIN_INCUBATOR_ID_LEN]; /* "" = desemparejado */
} app_evt_pairing_t;

/* Aviso de lo que vive el bebe: mask = bits de twin_notice_t; demo = viene de la demo (no se guarda). */
typedef struct {
    uint32_t mask;
    bool demo;
    char inc[TWIN_INCUBATOR_ID_LEN];
    char country[TWIN_COUNTRY_LEN]; /* pais de la incubadora; "" = no compartido */
    uint8_t sex;                    /* twin_sex_t del bebe (icono nino / nina) */
} app_evt_notice_t;

/* Novedades de recompensas (franja y «+N»); todo a 0 si solo cambio lo elegido */
typedef struct {
    uint8_t hearts;    /* corazones ganados ahora */
    uint8_t new_seals; /* bits de rw_seal_t */
    bool level_up;
    bool garland;
    bool demo;         /* del estado simulado de la demo */
} app_evt_rewards_t;

esp_err_t app_events_init(void);
esp_err_t app_events_post(app_event_id_t id, const void *data, size_t size);
esp_err_t app_events_subscribe(app_event_id_t id, esp_event_handler_t handler, void *arg);

#ifdef __cplusplus
}
#endif
