#include "usage_stats.h"

#include <Arduino.h>
#include <Preferences.h>

static uint32_t s_on, s_conn, s_hand, s_hand_n;
static bool s_hand_active = false;
static uint32_t s_last_save = 0;

#define SAVE_PERIOD_S 600 /* persistir cada 10 min para no desgastar flash */

static void save(void) {
    Preferences p;
    p.begin("usage", false);
    p.putULong("on", s_on);
    p.putULong("conn", s_conn);
    p.putULong("hand", s_hand);
    p.putULong("handn", s_hand_n);
    p.end();
}

void usage_init(void) {
    Preferences p;
    p.begin("usage", true);
    s_on = p.getULong("on", 0);
    s_conn = p.getULong("conn", 0);
    s_hand = p.getULong("hand", 0);
    s_hand_n = p.getULong("handn", 0);
    p.end();
}

void usage_tick_1s(bool incubator_connected) {
    s_on++;
    if (incubator_connected) s_conn++;
    if (s_hand_active) s_hand++;
    if (s_on - s_last_save >= SAVE_PERIOD_S) {
        s_last_save = s_on;
        save();
    }
}

void usage_hand_begin(void) {
    if (!s_hand_active) {
        s_hand_active = true;
        s_hand_n++;
    }
}

void usage_hand_end(void) { s_hand_active = false; }

uint32_t usage_on_s(void) { return s_on; }
uint32_t usage_conn_s(void) { return s_conn; }
uint32_t usage_hand_s(void) { return s_hand; }
uint32_t usage_hand_n(void) { return s_hand_n; }
