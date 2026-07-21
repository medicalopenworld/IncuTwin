#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Incubator usage states shown by the twin (no raw numbers on screen). */

typedef enum {
    THERMO_OFF = 0,
    THERMO_HEATING,
    THERMO_STABLE,
    THERMO_ALARM,
} thermo_state_t;

typedef struct {
    /* connectivity */
    bool wifi_connected;     /* panel joined WiFi                        */
    bool cloud_connected;    /* SSE stream to Firebase alive             */
    bool node_seen;          /* state node exists in Firebase (an
                              * IncuNest is linked to this panel)        */
    bool incubator_online;   /* IncuNest reporting to ThingsBoard        */

    /* incubator usage states */
    thermo_state_t thermo;   /* thermoregulation                         */
    bool phototherapy;       /* phototherapy lamp on                     */
    uint16_t heart_rate;     /* bpm, 0 = no pulse sensor / no signal     */
    bool baby_present;       /* baby inside ("baby" field; default true) */

    /* baby avatar */
    uint8_t skin_tone;       /* 0..5, set remotely (ThingsBoard attr)    */
    bool awake;              /* false = sleeping                         */

    uint32_t last_update_ms; /* millis() of last Firebase event          */
} twin_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Global state. Written by the network task, read by the UI.
 * Guarded by state_lock()/state_unlock(). */
extern twin_state_t g_state;
extern volatile bool g_state_dirty;

void state_init(void);
void state_lock(void);
void state_unlock(void);

#ifdef __cplusplus
}
#endif
