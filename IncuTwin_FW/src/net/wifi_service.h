#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Non-blocking WiFi manager: starts the connection and keeps retrying.
 * Updates g_state.wifi_connected. */
void wifi_service_start(void);

#ifdef __cplusplus
}
#endif
