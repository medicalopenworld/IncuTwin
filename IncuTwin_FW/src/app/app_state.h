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

/* Where the baby is ("baby" shared attribute pushed by ThingsBoard). */
typedef enum {
    BABY_NONE = 0, /* no episode: empty incubator                  */
    BABY_IN,       /* inside, receiving care                        */
    BABY_PARENTS,  /* out for a while with the parents (kangaroo)   */
    BABY_OUT,      /* episode closed (see "home" for discharge)     */
} baby_state_t;

typedef struct {
    /* connectivity */
    bool wifi_connected;     /* panel joined WiFi                        */
    bool cloud_connected;    /* MQTT session to ThingsBoard alive        */
    bool node_seen;          /* twin state received (an IncuNest is
                              * linked to this panel)                    */
    bool incubator_online;   /* IncuNest reporting to ThingsBoard        */

    /* incubator usage states */
    thermo_state_t thermo;   /* thermoregulation                         */
    bool phototherapy;       /* phototherapy lamp on                     */
    uint16_t heart_rate;     /* bpm, 0 = no pulse sensor / no signal     */
    baby_state_t baby;       /* "baby" field; default BABY_IN            */
    bool home;               /* with BABY_OUT: discharged home           */
    char baby_name[24];      /* "" = not shared (itw_show_name in TB)    */

    /* baby avatar */
    uint8_t skin_tone;       /* 0..5, set remotely (ThingsBoard attr)    */
    bool awake;              /* false = sleeping                         */

    uint32_t last_update_ms; /* millis() of last twin state update       */
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

/* Parse thermoregulation state from string.
 * Maps "heating"/"stable"/"alarm" to enum; unknown/NULL -> THERMO_OFF. */
thermo_state_t thermo_from_str(const char *s);

/* "none"/"in"/"parents"/"out"; unknown/NULL -> BABY_NONE. */
baby_state_t baby_from_str(const char *s);
const char *baby_to_str(baby_state_t b);

#ifdef __cplusplus
}
#endif
