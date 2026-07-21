#pragma once

/* Retro (8-bit style) buzzer notifications on the on-board buzzer.
 *
 * Non-blocking: a ~15 ms lv_timer steps through {freq, dur} sequences
 * with LEDC tones. Everything runs on the UI thread; call these only
 * from setup()/LVGL callbacks.
 *
 *   - boot jingle
 *   - "baby detected" fanfare on the (online && baby_present) rising
 *     edge — the same show_baby condition the UI uses — plus an
 *     audible heartbeat window of 10 s (the on-screen heart keeps
 *     beating on its own, always)
 *   - audible heartbeat while the baby is being touched
 *   - on/off toggle persisted in NVS ("incutwin"/"sound", default on)
 */

#include <stdbool.h>

#include "app/app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void sound_init(void);   /* LEDC channel + NVS + lv_timer */
void sound_set_enabled(bool on);
bool sound_is_enabled(void);

void sound_play_boot(void);
void sound_on_state(const twin_state_t *st); /* edge detect + bpm cache */
void sound_hand_hold(bool holding);          /* finger on the baby      */

#ifdef __cplusplus
}
#endif
