/* Payloads de incutwin/<id>/metrics (uso y recompensas, para ThingsBoard por el IoT Gateway) y de
 * incutwin/<id>/redeem (canje de «Una IncuNest con tu nombre"). C puro, probado en test_apps. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rewards_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define METRICS_PAYLOAD_MAX 896 /* con todo al maximo (test «pasaporte lleno cabe»): 787 */
#define REDEEM_PAYLOAD_MAX  96

typedef struct {
    int64_t ts;                          /* epoch; 0 sin hora */
    uint32_t on_s, conn_s, hand_s, hand_n, caress_n, tickle_n; /* usage_stats */
} metrics_usage_t;

/* Campos para analitica (los lee el recolector del VPS). */
typedef struct {
    uint32_t gen, boot_n, demo_n, q, lost;
    const char *fw, *inc, *phase;      /* phase: "off|searching|waiting|with|ending|farewell" */
    uint8_t lang, vol, bright;
} metrics_extra_t;

/* {"ts":..,"on_s":..,"conn_s":..,"hand_s":..,"hand_n":..,"car_n":..,"tic_n":..,"hearts":..,"earned":..,"level":..,
 *  "streak":..,"saved":..,"days":..,"babies":..,"seals":..,"seals_n":..,"countries":"TG,CM",
 *  "countries_n":..,"stars":..,"unlocked":..,"halo":..,"bg":..,"garland":..,"named_incunest":..,
 *  "hat_boy":..,"hat_girl":..,"blanket":..,"santa":0|1,"snow":0|1,"gen":..,"fw":"..","inc":"..",
 *  "phase":"..","lang":..,"vol":..,"bright":..,"demo_n":..,"boot_n":..,"q":..,"lost":..}
 * unlocked es de 64 bits (%llu). fw e inc solo admiten [A-Za-z0-9._-]; con otra cosa (o NULL) van
 * como "". Devuelve la longitud escrita o -1 si no cabe. */
int metrics_payload_build(char *out, size_t out_len, const metrics_usage_t *u, const rw_state_t *r,
                          const metrics_extra_t *x);

/* {"redeem":"named_incunest","hearts":365,"ts":..}. -1 si no cabe. */
int redeem_payload_build(char *out, size_t out_len, int64_t ts);

#ifdef __cplusplus
}
#endif
