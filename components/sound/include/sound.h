/* IncuTwin — notificaciones retro por el zumbador (spec sound).
 *
 * Melodias de onda cuadrada, latido "lub-dub" al ritmo del bebe y 4 niveles de
 * volumen (settings). La maquina de estados corre en un lv_timer de 15 ms en la
 * tarea de LVGL; las peticiones desde otras tareas se encolan de forma atomica.
 *
 * Escucha TWIN_EVT_TRANSITION (BABY_IN -> "Bebe detectado" + ventana de latido;
 * BABY_PARENTS -> fanfarria), TWIN_EVT_STATE_CHANGED (bpm visible) y
 * TWIN_EVT_SETTINGS_CHANGED (volumen).
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOUND_NONE = 0,
    SOUND_BOOT,    /* C5 E5 G5 C6 */
    SOUND_BABY,    /* G5 C6 E6 + 3 s de latido */
    SOUND_PARENTS, /* fanfarria */
    SOUND_TEST,    /* C6 120 ms */
    SOUND_CLICK,   /* tic de 25 ms al pulsar un boton */
} sound_melody_t;

/* Llamar con la tarea de LVGL ya creada (tras board_init). */
void sound_init(void);

/* Pide una melodia; segura desde cualquier tarea. Sustituye a la que suene. */
void sound_request(sound_melody_t m);

/* Dedo sobre el bebe / boton "Agarra mi mano" (desde la tarea de LVGL). */
void sound_hand_hold(bool holding);

#ifdef __cplusplus
}
#endif
