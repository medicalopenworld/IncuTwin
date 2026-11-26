/* Payloads de incutwin/<id>/metrics (uso y recompensas, para ThingsBoard por el IoT Gateway) y de
 * incutwin/<id>/redeem (canje de «Una IncuNest con tu nombre"). C puro, probado en test_apps. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rewards_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define METRICS_PAYLOAD_MAX 768
#define REDEEM_PAYLOAD_MAX  96

typedef struct {
    int64_t ts;                          /* epoch; 0 sin hora */
    uint32_t on_s, conn_s, hand_s, hand_n; /* usage_stats */
} metrics_usage_t;

/* {"ts":..,"on_s":..,"conn_s":..,"hand_s":..,"hand_n":..,"hearts":..,"earned":..,"level":..,
 *  "streak":..,"saved":..,"days":..,"babies":..,"seals":..,"seals_n":..,"countries":"TG,CM",
 *  "countries_n":..,"stars":..,"unlocked":..,"halo":..,"bg":..,"garland":..,"named_incunest":..,
 *  "hat_boy":..,"hat_girl":..,"blanket":..,"santa":0|1,"snow":0|1}
 * Devuelve la longitud escrita o -1 si no cabe. */
int metrics_payload_build(char *out, size_t out_len, const metrics_usage_t *u, const rw_state_t *r);

/* {"redeem":"named_incunest","hearts":365,"ts":..}. -1 si no cabe. */
int redeem_payload_build(char *out, size_t out_len, int64_t ts);

#ifdef __cplusplus
}
#endif
