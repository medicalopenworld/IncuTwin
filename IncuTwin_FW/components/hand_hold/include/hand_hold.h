/* IncuTwin — gesto "Coge mi mano" (spec hold-my-hand). C puro, sin LVGL ni red:
 * el tiempo entra como parametro (ms monotonos, se tolera el desbordamiento de 32 bits).
 * Lo lleva la tarea de LVGL; probado en test_apps. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HH_IDLE = 0, /* reposo */
    HH_READY,    /* boton pulsado, esperando el dedo */
    HH_HOLDING,  /* dedo puesto, acumulando */
    HH_GRACE,    /* dedo levantado antes de completar */
    HH_DONE,     /* completado y con el dedo puesto */
    HH_THANKS,   /* completado y dedo levantado: agradecimiento */
} hh_state_t;

typedef struct {
    uint32_t min_ms;      /* dedo acumulado para completar */
    uint32_t grace_ms;    /* para volver a poner el dedo */
    uint32_t ready_ms;    /* esperando el dedo tras el boton */
    uint32_t thanks_ms;   /* agradecimiento tras soltar */
    uint32_t cooldown_ms; /* entre avisos publicados; 0 = sin espera */
} hh_config_t;

typedef struct {
    hh_config_t cfg;
    hh_state_t state;
    uint32_t acc_ms;     /* dedo acumulado en el gesto actual */
    uint32_t last_ms;    /* ultima marca de acumulacion */
    uint32_t enter_ms;   /* entrada en el estado actual */
    uint32_t sent_ms;    /* ultimo aviso publicado */
    bool ever_sent;
    bool can_send;
    bool send_pending;
} hand_hold_t;

void hh_init(hand_hold_t *h, const hh_config_t *cfg);

/* Entradas */
void hh_button(hand_hold_t *h, uint32_t now);
void hh_touch(hand_hold_t *h, bool down, uint32_t now);
/* visible = vista BEBE en pantalla (si no, vuelve a reposo);
 * can_send = ni demo, con sesion MQTT y emparejado. */
void hh_set_available(hand_hold_t *h, bool visible, bool can_send, uint32_t now);
void hh_tick(hand_hold_t *h, uint32_t now);

/* Salidas */
static inline hh_state_t hh_state(const hand_hold_t *h) { return h->state; }
uint16_t hh_progress_permille(const hand_hold_t *h, uint32_t now); /* 0..1000 hasta completar */
/* Dedo acumulado en el gesto, tambien despues de completar (0 en reposo). Con min_ms da
 * las vueltas del anillo: vuelta = held / min_ms. */
uint32_t hh_held_ms(const hand_hold_t *h, uint32_t now);
uint8_t hh_grace_left_s(const hand_hold_t *h, uint32_t now); /* 0 fuera de HH_GRACE */
/* true una sola vez por aviso que hay que publicar */
bool hh_take_send(hand_hold_t *h);

#ifdef __cplusplus
}
#endif
