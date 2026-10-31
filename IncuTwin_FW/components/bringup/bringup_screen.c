#include "bringup_screen.h"

#include "sdkconfig.h"

#if CONFIG_INCUTWIN_BRINGUP_SCREEN

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"

static const char *TAG = "bringup";

#define TARGET_R      12
#define HIT_TOL       18 /* un dedo no es un lapiz: la spec pide <= 6 px de error del
                            punto, pero el centro del contacto se desvia mas */
#define TARGET_MARGIN 20
#define N_TARGETS     5

typedef struct {
    int16_t x, y;
    lv_obj_t *obj;
} target_t;

static target_t s_targets[N_TARGETS] = {
    { TARGET_MARGIN, TARGET_MARGIN, NULL },
    { 240 - TARGET_MARGIN, TARGET_MARGIN, NULL },
    { TARGET_MARGIN, 320 - TARGET_MARGIN, NULL },
    { 240 - TARGET_MARGIN, 320 - TARGET_MARGIN, NULL },
    { 120, 160, NULL },
};

static lv_obj_t *s_lbl_touch_cfg;
static lv_obj_t *s_lbl_last;
static lv_obj_t *s_lbl_mem;
static lv_obj_t *s_marker; /* punto coral en el ultimo toque registrado */
static uint8_t s_touch_combo; /* bit0 swap_xy, bit1 mirror_x, bit2 mirror_y */
static int64_t s_press_t0_us;

/* ---------------------------------------------------------------- touch cfg */

static void touch_combo_apply(void)
{
    esp_lcd_touch_handle_t tp = board_touch_handle();
    bool swap = s_touch_combo & 1, mx = s_touch_combo & 2, my = s_touch_combo & 4;
    if (tp) {
        esp_lcd_touch_set_swap_xy(tp, swap);
        esp_lcd_touch_set_mirror_x(tp, mx);
        esp_lcd_touch_set_mirror_y(tp, my);
    }
    lv_label_set_text_fmt(s_lbl_touch_cfg, "touch %s  combo %u: swap=%d mx=%d my=%d",
                          board_touch_chip_name(), s_touch_combo, swap, mx, my);
    ESP_LOGI(TAG, "touch combo %u: swap_xy=%d mirror_x=%d mirror_y=%d", s_touch_combo, swap, mx, my);
}

static void touch_combo_from_kconfig(void)
{
    bool swap = false, mx = false, my = false;
    board_touch_default_flags(&swap, &mx, &my);
    s_touch_combo = (swap ? 1 : 0) | (mx ? 2 : 0) | (my ? 4 : 0);
}

/* ------------------------------------------------------------------- events */

static void screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p = { 0, 0 };
    if (indev) {
        lv_indev_get_point(indev, &p);
    }
    int64_t now = esp_timer_get_time();

    if (code == LV_EVENT_PRESSED) {
        s_press_t0_us = now;
        int best = -1, best_err = 1 << 30;
        for (int i = 0; i < N_TARGETS; i++) {
            int dx = p.x - s_targets[i].x, dy = p.y - s_targets[i].y;
            int err = dx * dx + dy * dy;
            if (err < best_err) {
                best_err = err;
                best = i;
            }
        }
        bool hit = best >= 0 && best_err <= HIT_TOL * HIT_TOL;
        if (hit) {
            lv_obj_set_style_bg_color(s_targets[best].obj, lv_color_hex(0x3F9E63), 0);
        }
        lv_obj_set_pos(s_marker, p.x - 5, p.y - 5);
        lv_obj_set_hidden(s_marker, false);
        int dx = best >= 0 ? p.x - s_targets[best].x : 0;
        int dy = best >= 0 ? p.y - s_targets[best].y : 0;
        ESP_LOGI(TAG, "touch PRESSED (%d,%d) diana %d dx=%+d dy=%+d %s", (int)p.x, (int)p.y, best,
                 dx, dy, hit ? "ACIERTO" : "fuera");
        lv_label_set_text_fmt(s_lbl_last, "toque (%d,%d) diana %d %+d,%+d %s", (int)p.x, (int)p.y,
                              best, dx, dy, hit ? "OK" : "fuera");
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ESP_LOGI(TAG, "touch RELEASED tras %lld ms", (long long)((now - s_press_t0_us) / 1000));
    }
}

/* ------------------------------------------------------------------ buzzer */

static const uint16_t SCALE_HZ[] = { 523, 659, 784, 1047, 0 };
static uint8_t s_scale_idx;

static void scale_timer_cb(lv_timer_t *t)
{
    if (s_scale_idx >= sizeof(SCALE_HZ) / sizeof(SCALE_HZ[0])) {
        board_buzzer_tone(0, 0);
        lv_timer_delete(t);
        return;
    }
    board_buzzer_tone(SCALE_HZ[s_scale_idx], 512);
    s_scale_idx++;
}

static void scale_play(void)
{
    s_scale_idx = 0;
    lv_timer_create(scale_timer_cb, 150, NULL);
    ESP_LOGI(TAG, "zumbador: escala C5 E5 G5 C6");
}

/* ------------------------------------------------------------------ timers */

/* Boton BOOT: muestreo cada 20 ms, antirrebote por nivel estable, corta/larga. */
static void button_timer_cb(lv_timer_t *t)
{
    (void)t;
    static bool pressed, long_done;
    static int64_t t_press;
    static uint8_t stable;
    bool raw = board_button_pressed();
    stable = raw == pressed ? 0 : stable + 1;
    if (stable >= 2) { /* 40 ms estable */
        stable = 0;
        pressed = raw;
        if (pressed) {
            t_press = esp_timer_get_time();
            long_done = false;
            ESP_LOGI(TAG, "BOOT pressed");
        } else {
            ESP_LOGI(TAG, "BOOT released");
            if (!long_done) {
                s_touch_combo = (s_touch_combo + 1) & 7;
                touch_combo_apply();
            }
        }
    }
    if (pressed && !long_done && esp_timer_get_time() - t_press >= 1000 * 1000) {
        long_done = true;
        scale_play();
    }
}

/* Cada segundo: rampa de brillo y memoria. */
static void second_timer_cb(lv_timer_t *t)
{
    (void)t;
    static const uint8_t RAMP[] = { 10, 50, 100 };
    static uint8_t i;
    board_backlight_set(RAMP[i]);
    i = (i + 1) % 3;
    lv_label_set_text_fmt(s_lbl_mem, "brillo %u%%  heap int %u KB  psram %u KB",
                          board_backlight_get(),
                          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
}

static void mover_anim_cb(void *var, int32_t v) { lv_obj_set_x((lv_obj_t *)var, v); }

/* ------------------------------------------------------------------- build */

void bringup_screen_start(void)
{
    if (!lvgl_port_lock(1000)) {
        ESP_LOGE(TAG, "no se pudo coger el lock de LVGL");
        return;
    }
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF4F3F0), 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_PRESS_LOST, NULL);

    /* orientacion */
    lv_obj_t *red = lv_obj_create(scr);
    lv_obj_set_size(red, 40, 40);
    lv_obj_set_pos(red, 0, 0);
    lv_obj_set_style_bg_color(red, lv_color_hex(0xE0524A), 0);
    lv_obj_set_style_border_width(red, 0, 0);
    lv_obj_set_style_radius(red, 0, 0);
    lv_obj_set_clickable(red, false);
    lv_obj_set_scrollable(red, false);

    lv_obj_t *green = lv_obj_create(scr);
    lv_obj_set_size(green, 40, 40);
    lv_obj_set_pos(green, 200, 280);
    lv_obj_set_style_bg_color(green, lv_color_hex(0x3F9E63), 0);
    lv_obj_set_style_border_width(green, 0, 0);
    lv_obj_set_style_radius(green, 0, 0);
    lv_obj_set_clickable(green, false);
    lv_obj_set_scrollable(green, false);

    lv_obj_t *usb = lv_label_create(scr);
    lv_label_set_text(usb, "USB " LV_SYMBOL_UP);
    lv_obj_set_style_text_font(usb, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(usb, lv_color_hex(0x054E92), 0);
    lv_obj_align(usb, LV_ALIGN_TOP_MID, 0, 50);

    /* dianas */
    for (int i = 0; i < N_TARGETS; i++) {
        lv_obj_t *c = lv_obj_create(scr);
        lv_obj_set_size(c, TARGET_R * 2, TARGET_R * 2);
        lv_obj_set_pos(c, s_targets[i].x - TARGET_R, s_targets[i].y - TARGET_R);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(c, 3, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x054E92), 0);
        lv_obj_set_clickable(c, false);
        lv_obj_set_scrollable(c, false);
        lv_obj_t *n = lv_label_create(c);
        lv_label_set_text_fmt(n, "%d", i);
        lv_obj_set_style_text_font(n, &lv_font_montserrat_12, 0);
        lv_obj_center(n);
        s_targets[i].obj = c;
    }

    /* circulo en movimiento (tearing) */
    lv_obj_t *mover = lv_obj_create(scr);
    lv_obj_set_size(mover, 20, 20);
    lv_obj_set_pos(mover, 0, 110);
    lv_obj_set_style_radius(mover, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(mover, lv_color_hex(0xDE6F63), 0);
    lv_obj_set_style_border_width(mover, 0, 0);
    lv_obj_set_clickable(mover, false);
    lv_obj_set_scrollable(mover, false);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, mover);
    lv_anim_set_exec_cb(&a, mover_anim_cb);
    lv_anim_set_values(&a, 0, 220);
    lv_anim_set_duration(&a, 1500);
    lv_anim_set_playback_duration(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* marcador del ultimo toque */
    s_marker = lv_obj_create(scr);
    lv_obj_set_size(s_marker, 10, 10);
    lv_obj_set_style_radius(s_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_marker, lv_color_hex(0xDE6F63), 0);
    lv_obj_set_style_border_width(s_marker, 0, 0);
    lv_obj_set_clickable(s_marker, false);
    lv_obj_set_scrollable(s_marker, false);
    lv_obj_set_hidden(s_marker, true);

    /* textos de estado */
    s_lbl_touch_cfg = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_touch_cfg, &lv_font_montserrat_12, 0);
    lv_obj_set_width(s_lbl_touch_cfg, 200);
    lv_label_set_long_mode(s_lbl_touch_cfg, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl_touch_cfg, LV_ALIGN_CENTER, 0, 40);

    s_lbl_last = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_last, &lv_font_montserrat_12, 0);
    lv_label_set_text(s_lbl_last, "toca una diana");
    lv_obj_align(s_lbl_last, LV_ALIGN_CENTER, 0, 80);

    s_lbl_mem = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_mem, &lv_font_montserrat_12, 0);
    lv_obj_set_width(s_lbl_mem, 200);
    lv_label_set_long_mode(s_lbl_mem, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl_mem, LV_ALIGN_CENTER, 0, 110);

    lv_obj_t *help = lv_label_create(scr);
    lv_obj_set_style_text_font(help, &lv_font_montserrat_12, 0);
    lv_label_set_text(help, "BOOT corto: combo tactil\nBOOT largo: escala");
    lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -50);

    touch_combo_from_kconfig();
    touch_combo_apply();

    lv_timer_create(button_timer_cb, 20, NULL);
    lv_timer_create(second_timer_cb, 1000, NULL);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "pantalla de bring-up lista");
}

#else /* !CONFIG_INCUTWIN_BRINGUP_SCREEN */

void bringup_screen_start(void) {}

#endif
