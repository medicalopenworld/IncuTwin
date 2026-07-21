#include "baby_widget.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

#include "assets/assets.h"
#include "assets/fonts/fonts_es.h"
#include "theme.h"

#define PALETTE_BYTES (256 * 4)

typedef enum { FR_SLEEP = 0, FR_YAWN1, FR_YAWN2, FR_AWAKE, FR_COUNT } frame_t;

/* PSRAM copies of the frames so the palette can be rewritten at runtime */
static lv_img_dsc_t s_frames[FR_COUNT];
static const lv_img_dsc_t *S_SRC[FR_COUNT] = {&img_baby_sleep, &img_baby_yawn1,
                                              &img_baby_yawn2, &img_baby_awake};

static lv_obj_t *s_img = nullptr;
static lv_obj_t *s_heart = nullptr;
static lv_obj_t *s_zzz[3] = {nullptr, nullptr, nullptr};

static uint8_t s_tone = 0;
static bool s_awake = false;
static uint16_t s_bpm = 0;
static bool s_yawning = false;

static lv_timer_t *s_yawn_timer = nullptr; /* schedules the next yawn      */
static lv_timer_t *s_seq_timer = nullptr;  /* steps through yawn frames    */
static int s_seq_step = 0;

/* ------------------------------------------------------------------ frames */

static void frames_init(void) {
    for (int i = 0; i < FR_COUNT; i++) {
        uint8_t *buf =
            (uint8_t *)heap_caps_malloc(S_SRC[i]->data_size, MALLOC_CAP_SPIRAM);
        if (!buf) { /* fallback: point at flash (skin tone fixed) */
            s_frames[i] = *S_SRC[i];
            continue;
        }
        memcpy(buf, S_SRC[i]->data, S_SRC[i]->data_size);
        s_frames[i] = *S_SRC[i];
        s_frames[i].data = buf;
    }
}

static void apply_palette(uint8_t tone) {
    if (tone >= BABY_SKIN_TONE_COUNT) tone = 0;
    for (int i = 0; i < FR_COUNT; i++) {
        if (s_frames[i].data == S_SRC[i]->data) continue; /* flash fallback */
        memcpy((void *)s_frames[i].data, baby_skin_palettes[tone],
               PALETTE_BYTES);
        lv_img_cache_invalidate_src(&s_frames[i]);
    }
    if (s_img) lv_obj_invalidate(s_img);
}

static void show_frame(frame_t f) {
    if (s_img) lv_img_set_src(s_img, &s_frames[f]);
}

static frame_t rest_frame(void) { return s_awake ? FR_AWAKE : FR_SLEEP; }

/* ------------------------------------------------------------------- yawns */

static void seq_timer_cb(lv_timer_t *t) {
    static const frame_t SEQ[] = {FR_YAWN1, FR_YAWN2, FR_YAWN2, FR_YAWN1};
    static const uint32_t DUR[] = {320, 900, 350, 320};

    if (s_seq_step < (int)(sizeof(SEQ) / sizeof(SEQ[0]))) {
        show_frame(SEQ[s_seq_step]);
        lv_timer_set_period(t, DUR[s_seq_step]);
        s_seq_step++;
    } else {
        show_frame(rest_frame());
        s_yawning = false;
        lv_timer_del(t);
        s_seq_timer = nullptr;
    }
}

static void start_yawn(void) {
    if (s_yawning || s_seq_timer) return;
    s_yawning = true;
    s_seq_step = 0;
    s_seq_timer = lv_timer_create(seq_timer_cb, 10, nullptr);
}

static void yawn_scheduler_cb(lv_timer_t *t) {
    start_yawn();
    /* next yawn in 18..40 s */
    lv_timer_set_period(t, 18000 + (uint32_t)random(22000));
}

/* --------------------------------------------------------------- breathing */

static void breath_anim_cb(void *var, int32_t v) {
    lv_img_set_zoom((lv_obj_t *)var, (uint16_t)v);
}

static void start_breathing(void) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_img);
    lv_anim_set_exec_cb(&a, breath_anim_cb);
    lv_anim_set_values(&a, 256, 271);
    lv_anim_set_time(&a, 2600);
    lv_anim_set_playback_time(&a, 2600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* ------------------------------------------------------------------- heart */

static void heart_anim_cb(void *var, int32_t v) {
    lv_img_set_zoom((lv_obj_t *)var, (uint16_t)v);
}

static void restart_heart_anim(void) {
    lv_anim_del(s_heart, heart_anim_cb);
    if (s_bpm == 0) {
        lv_obj_add_flag(s_heart, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_heart, LV_OBJ_FLAG_HIDDEN);

    uint32_t period = 60000UL / s_bpm; /* full beat */
    if (period < 250) period = 250;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_heart);
    lv_anim_set_exec_cb(&a, heart_anim_cb);
    lv_anim_set_values(&a, 190, 256);
    lv_anim_set_time(&a, period / 3);
    lv_anim_set_playback_time(&a, (period * 2) / 3);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* --------------------------------------------------------------------- Zzz */

static void zzz_anim_y(void *var, int32_t v) {
    lv_obj_set_y((lv_obj_t *)var, v);
}
static void zzz_anim_opa(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void zzz_create(lv_obj_t *parent, lv_coord_t bx, lv_coord_t by) {
    static const char *TXT[3] = {"z", "Z", "z"};
    static const lv_coord_t DX[3] = {66, 82, 98};
    static const lv_coord_t DY[3] = {30, 14, -2};

    for (int i = 0; i < 3; i++) {
        s_zzz[i] = lv_label_create(parent);
        lv_label_set_text(s_zzz[i], TXT[i]);
        lv_obj_set_style_text_color(s_zzz[i], COL_NAVY, 0);
        lv_obj_set_style_text_font(
            s_zzz[i], i == 1 ? &lv_font_es_20 : &lv_font_es_14,
            0);
        lv_obj_set_pos(s_zzz[i], bx + DX[i], by + DY[i]);

        lv_coord_t y0 = by + DY[i];

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_zzz[i]);
        lv_anim_set_exec_cb(&a, zzz_anim_y);
        lv_anim_set_values(&a, y0, y0 - 14);
        lv_anim_set_time(&a, 2400);
        lv_anim_set_delay(&a, i * 800);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);

        lv_anim_set_exec_cb(&a, zzz_anim_opa);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_start(&a);
    }
}

static void zzz_set_visible(bool vis) {
    for (int i = 0; i < 3; i++) {
        if (!s_zzz[i]) continue;
        if (vis)
            lv_obj_clear_flag(s_zzz[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_zzz[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* --------------------------------------------------------------------- API */

void baby_widget_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y) {
    frames_init();

    s_img = lv_img_create(parent);
    lv_img_set_src(s_img, &s_frames[FR_SLEEP]);
    lv_obj_set_pos(s_img, x, y);
    lv_img_set_pivot(s_img, s_frames[FR_SLEEP].header.w / 2,
                     s_frames[FR_SLEEP].header.h / 2);

    s_heart = lv_img_create(parent);
    lv_img_set_src(s_heart, &img_heart);
    lv_obj_align_to(s_heart, s_img, LV_ALIGN_CENTER, 2, 34);
    lv_obj_add_flag(s_heart, LV_OBJ_FLAG_HIDDEN);

    zzz_create(parent, x, y);

    apply_palette(s_tone);
    start_breathing();

    s_yawn_timer =
        lv_timer_create(yawn_scheduler_cb, 15000 + (uint32_t)random(10000),
                        nullptr);
}

void baby_set_skin_tone(uint8_t tone) {
    if (tone >= BABY_SKIN_TONE_COUNT || tone == s_tone) return;
    s_tone = tone;
    apply_palette(tone);
}

uint8_t baby_get_skin_tone(void) { return s_tone; }

void baby_set_awake(bool awake) {
    if (awake == s_awake) return;
    s_awake = awake;
    if (!s_yawning) show_frame(rest_frame());
    zzz_set_visible(!awake);
}

void baby_set_heart_rate(uint16_t bpm) {
    if (bpm == s_bpm) return;
    s_bpm = bpm;
    restart_heart_anim();
}

void baby_trigger_yawn(void) { start_yawn(); }

lv_obj_t *baby_widget_obj(void) { return s_img; }
