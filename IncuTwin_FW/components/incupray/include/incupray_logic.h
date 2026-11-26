/* IncuTwin — IncuPray (spec incupray): la ventana semanal, los avisos que tocan, la entrada con
 * apuesta, los corazones por minuto y los mensajes del broker. Modulo puro (sin NVS, FreeRTOS ni
 * red), probado en test_apps. Los textos de la IncuPray nunca hablan de rezar. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "twin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IP_ID_MAX            23
#define IP_NAMES_MAX         40  /* nombres que caben en un incupray/live */
#define IP_NAME_BYTES        49  /* 16 caracteres UTF-8 (hasta 3 bytes) + 0 */
#define IP_BABIES_MAX        24
#define IP_STAKE             3   /* corazones al entrar, si los hay */
#define IP_HEARTS_MAX        10  /* por IncuPray */
#define IP_SECONDS_PER_HEART 60
#define IP_EVE_S             (24u * 3600u)
#define IP_HOUR_S            3600u
#define IP_PARTICIPATION_MAX 96

/* incupray/next (retenido) */
typedef struct {
    char id[IP_ID_MAX + 1];
    uint32_t start;      /* epoch */
    uint32_t duration_s;
} ip_window_t;

typedef enum { IP_BEFORE = 0, IP_OPEN, IP_AFTER } ip_phase_t;

typedef enum {
    IP_NOTICE_NONE = 0,
    IP_NOTICE_EVE = 1,   /* la vispera */
    IP_NOTICE_HOUR = 2,  /* una hora antes */
    IP_NOTICE_START = 4, /* empieza */
} ip_notice_t;

typedef struct {
    char inc[TWIN_INCUBATOR_ID_LEN];
    char country[TWIN_COUNTRY_LEN];
    uint8_t sex;  /* twin_sex_t */
    uint8_t skin; /* 0..4 */
} ip_baby_t;

/* incupray/live: cada 10 s durante la ventana */
typedef struct {
    char id[IP_ID_MAX + 1];
    uint16_t count;  /* IncuTwin unidas */
    uint32_t hearts; /* corazones enviados entre todas */
    uint8_t n_names;
    char names[IP_NAMES_MAX][IP_NAME_BYTES];
    uint8_t n_babies;
    ip_baby_t babies[IP_BABIES_MAX];
} ip_live_t;

/* incupray/summary (retenido, al cerrar) */
typedef struct {
    char id[IP_ID_MAX + 1];
    uint16_t count;
    uint32_t hearts;
} ip_summary_t;

ip_phase_t ip_phase(const ip_window_t *w, uint32_t now);
uint32_t ip_remaining_s(const ip_window_t *w, uint32_t now); /* 0 fuera de la ventana */
/* El aviso que toca ahora y aun no se ha enseñado (`shown` = mascara de ip_notice_t); cada uno solo
 * en su tramo: vispera (de -24 h a -1 h), una hora antes (de -1 h al inicio), empieza (ventana). */
ip_notice_t ip_notice_due(const ip_window_t *w, uint32_t now, uint8_t shown);
uint16_t ip_stake(uint16_t balance);  /* 3 si hay, 0 si no: nadie se queda fuera */
uint8_t ip_hearts_for(uint32_t held_s); /* 1 por minuto de dedo, hasta IP_HEARTS_MAX */

bool ip_parse_next(const char *payload, size_t len, ip_window_t *out);
bool ip_parse_live(const char *payload, size_t len, ip_live_t *out);
bool ip_parse_summary(const char *payload, size_t len, ip_summary_t *out);
/* {"id":..,"joined":true,"held_s":..,"hearts":..} -> longitud o -1 si no cabe */
int ip_participation_build(char *out, size_t n, const char *id, uint32_t held_s, uint8_t hearts);

#ifdef __cplusplus
}
#endif
