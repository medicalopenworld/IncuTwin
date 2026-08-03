#pragma once

/* Firebase Realtime Database SSE streaming client.
 *
 * Subscribes to FIREBASE_STATE_PATH and applies every "put"/"patch" event
 * to g_state. Runs in its own FreeRTOS task (core 0), so the UI (core 1)
 * never blocks on network I/O.
 */

#ifdef __cplusplus
extern "C" {
#endif

void firebase_stream_start(void);

#ifdef __cplusplus
}
#endif
