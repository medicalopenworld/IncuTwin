/* IncuTwin — digital twin display for the IncuNest incubator.
 *
 * Board: Elecrow CrowPanel Advance 2.8" (ESP32-S3, ST7789 320x240,
 * FT5x06/FT6336 touch — confirmed via I2C scan on this unit; Elecrow
 * ships this line with either that chip or a GT911 depending on batch).
 * Data:  ThingsBoard -> Firebase RTDB -> this panel (SSE streaming).
 */

#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "app/app_state.h"
#include "app/identity.h"
#include "app/usage_stats.h"
#include "config.h"
#include "display/LGFX_CrowPanel28.h"
#include "net/firebase_stream.h"
#include "net/sim_server.h"
#include "net/tb_client.h"
#include "net/wifi_service.h"
#include "pins_config.h"
#include "sound/sound.h"
#include "ui/onboarding.h"
#include "ui/ui_main.h"

// #define TOUCH_DEBUG

static LGFX_CrowPanel28 gfx;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

/* ---------------------------------------------------------------- display */

static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p) {
    if (gfx.getStartCount() > 0) gfx.endWrite();
    gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1,
                     (lgfx::rgb565_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

/* ------------------------------------------------------------------ touch */

static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    data->state = LV_INDEV_STATE_REL;

    uint16_t touch_x, touch_y;
    if (!gfx.getTouch(&touch_x, &touch_y)) return;

    /* Portrait at offset_rotation 2: the panel's 180 deg flip is not
     * seen by the touch pipeline, so mirror both axes of the previous
     * portrait mapping (which was x' = x, y' = H - y). */
    data->state = LV_INDEV_STATE_PR;
    data->point.x = constrain(LCD_H_RES - touch_x, 0, LCD_H_RES - 1);
    data->point.y = constrain(touch_y, 0, LCD_V_RES - 1);

#ifdef TOUCH_DEBUG
    Serial.printf("touch raw=(%d,%d) -> (%d,%d)\n", touch_x, touch_y,
                  data->point.x, data->point.y);
#endif
}

/* ------------------------------------------------------------------ setup */

void setup() {
    Serial.begin(115200);

    state_init();
    identity_init();
    usage_init();

    pinMode(18, OUTPUT);

    gfx.init();
    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

    /* backlight (PWM so it could be dimmed later) */
    ledcSetup(0, 5000, 8);
    ledcAttachPin(LCD_PIN_BL, 0);
    ledcWrite(0, 255);

    lv_init();
    size_t buffer_size = sizeof(lv_color_t) * LCD_H_RES * LCD_V_RES;
    buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_H_RES * LCD_V_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);

    sound_init();

    if (!prov_is_done()) {
        /* primer arranque: asistente de onboarding.
         * La tarea de ThingsBoard se provisiona sola al haber WiFi. */
        ui_onboarding_start();
#ifndef SIM_MODE
        tb_client_start();
#endif
        Serial.printf("IncuTwin %s (SN %s) onboarding\n", FW_VERSION,
                      identity_sn());
    } else {
        ui_init();
        wifi_service_start();
#ifdef SIM_MODE
        sim_server_start();
#else
        firebase_stream_start();
#endif
#ifndef SIM_MODE
        tb_client_start();
#endif
        Serial.printf("IncuTwin %s (SN %s) ready\n", FW_VERSION,
                      identity_sn());
    }

    sound_play_boot();
}

void loop() {
    lv_timer_handler();

    /* contadores de uso: tick de 1 s */
    static uint32_t last_tick = 0;
    if (millis() - last_tick >= 1000) {
        last_tick = millis();
        state_lock();
        bool online = g_state.incubator_online && g_state.cloud_connected;
        state_unlock();
        usage_tick_1s(online);
    }
    delay(5);
}
