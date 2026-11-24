#include "baby_widget.h"

#include <string.h>

#include "assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui_theme.h"

static const char *TAG = "baby";

typedef enum { FR_SLEEP = 0, FR_YAWN1, FR_YAWN2, FR_AWAKE, FR_COUNT } frame_t;

/* Dos dibujos, nino y nina (misma figura y caja: los fotogramas encajan pixel a pixel).
 * Comparten paleta, asi que el tono de piel se aplica igual a los dos. Copias en PSRAM
 * para poder reescribir la paleta en caliente (~180 KB los ocho). */
enum { DRAW_BOY = 0, DRAW_GIRL, DRAW_COUNT };
static lv_image_dsc_t s_frames[DRAW_COUNT][FR_COUNT];
static const lv_image_dsc_t *const S_SRC[DRAW_COUNT][FR_COUNT] = {
    { &img_baby_sleep, &img_baby_yawn1, &img_baby_yawn2, &img_baby_awake },
    { &img_baby_girl_sleep, &img_baby_girl_yawn1, &img_baby_girl_yawn2, &img_baby_girl_awake },
};

static lv_obj_t *s_img;
static lv_obj_t *s_heart;
static lv_obj_t *s_zzz[3];
static uint8_t s_tone;
static uint8_t s_draw = DRAW_BOY;
static frame_t s_cur = FR_SLEEP;
static bool s_awake;
static uint16_t s_bpm;
static bool s_yawning;
static lv_timer_t *s_seq_timer;
static int s_seq_step;

/* ---------------------------------------------------------------- frames */

static void frames_init(void)
{
    for (int d = 0; d < DRAW_COUNT; d++) {
        for (int i = 0; i < FR_COUNT; i++) {
            s_frames[d][i] = *S_SRC[d][i];
            uint8_t *buf = heap_caps_malloc(S_SRC[d][i]->data_size, MALLOC_CAP_SPIRAM);
            if (!buf) { /* sin PSRAM: se queda en flash, tono fijo */
                ESP_LOGW(TAG, "sin PSRAM para el fotograma %d/%d", d, i);
                continue;
            }
            memcpy(buf, S_SRC[d][i]->data, S_SRC[d][i]->data_size);
            s_frames[d][i].data = buf;
        }
    }
}

static void apply_palette(uint8_t tone)
{
    if (tone >= BABY_SKIN_TONE_COUNT) tone = 0;
    for (int d = 0; d < DRAW_COUNT; d++) {
        for (int i = 0; i < FR_COUNT; i++) {
            if (s_frames[d][i].data == S_SRC[d][i]->data) continue; /* copia en flash */
            memcpy((void *)s_frames[d][i].data, baby_skin_palettes[tone], BABY_PALETTE_BYTES);
#if LV_CACHE_DEF_SIZE > 0
            lv_image_cache_drop(&s_frames[d][i]); /* la cache guardaria la paleta vieja */
#endif
        }
    }
    if (s_img) lv_obj_invalidate(s_img);
}

static void show_frame(frame_t f)
{
    s_cur = f;
    if (s_img) lv_image_set_src(s_img, &s_frames[s_draw][f]);
}

static frame_t rest_frame(void) { return s_awake ? FR_AWAKE : FR_SLEEP; }

/* ----------------------------------------------------------------- yawns */

static void seq_timer_cb(lv_timer_t *t)
{
    static const frame_t SEQ[] = { FR_YAWN1, FR_YAWN2, FR_YAWN2, FR_YAWN1 };
    static const uint32_t DUR[] = { 320, 900, 350, 320 };
    if (s_seq_step < 4) {
        show_frame(SEQ[s_seq_step]);
        lv_timer_set_period(t, DUR[s_seq_step]);
        s_seq_step++;
    } else {
        show_frame(rest_frame());
        s_yawning = false;
        lv_timer_delete(t);
        s_seq_timer = NULL;
    }
}

static void start_yawn(void)
{
    if (s_yawning || s_seq_timer) return;
    s_yawning = true;
    s_seq_step = 0;
    s_seq_timer = lv_timer_create(seq_timer_cb, 10, NULL);
}

static void yawn_scheduler_cb(lv_timer_t *t)
{
    start_yawn();
    lv_timer_set_period(t, lv_rand(18000, 40000)); /* siguiente bostezo en 18..40 s */
}

/* ------------------------------------------------------------- breathing */

static void anim_y_cb(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }
static void anim_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

static void start_breathing(void)
{
    int32_t y0 = lv_obj_get_y(s_img);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_img);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_values(&a, y0, y0 - 3);
    lv_anim_set_duration(&a, 2600);
    lv_anim_set_reverse_duration(&a, 2600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* ----------------------------------------------------------------- heart */

static void restart_heart_anim(void)
{
    lv_anim_delete(s_heart, anim_opa_cb);
    if (s_bpm == 0) {
        lv_obj_set_hidden(s_heart, true);
        return;
    }
    lv_obj_set_hidden(s_heart, false);
    uint32_t period = 60000UL / s_bpm;
    if (period < 250) period = 250;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_heart);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_duration(&a, period / 3);
    lv_anim_set_reverse_duration(&a, (period * 2) / 3);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* ------------------------------------------------------------------- Zzz */

static void zzz_create(lv_obj_t *parent, int32_t bx, int32_t by)
{
    static const char *const TXT[3] = { "z", "Z", "z" };
    static const int32_t DX[3] = { 66, 82, 98 };
    static const int32_t DY[3] = { 30, 14, -2 };

    for (int i = 0; i < 3; i++) {
        s_zzz[i] = lv_label_create(parent);
        lv_label_set_text(s_zzz[i], TXT[i]);
        lv_obj_set_style_text_color(s_zzz[i], COL_NAVY, 0);
        lv_obj_set_style_text_font(s_zzz[i], i == 1 ? &lv_font_es_20 : &lv_font_es_14, 0);
        lv_obj_set_pos(s_zzz[i], bx + DX[i], by + DY[i]);
        int32_t y0 = by + DY[i];

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_zzz[i]);
        lv_anim_set_exec_cb(&a, anim_y_cb);
        lv_anim_set_values(&a, y0, y0 - 14);
        lv_anim_set_duration(&a, 2400);
        lv_anim_set_delay(&a, i * 800);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);

        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_start(&a);
    }
}

static void zzz_set_visible(bool vis)
{
    for (int i = 0; i < 3; i++) {
        if (s_zzz[i]) lv_obj_set_hidden(s_zzz[i], !vis);
    }
}

/* ------------------------------------------------------------------- API */

lv_obj_t *baby_widget_create(lv_obj_t *parent, int32_t x, int32_t y)
{
    frames_init();

    s_img = lv_image_create(parent);
    lv_image_set_src(s_img, &s_frames[s_draw][FR_SLEEP]);
    lv_obj_set_pos(s_img, x, y);

    s_heart = lv_image_create(parent);
    lv_image_set_src(s_heart, &img_heart);
    lv_obj_align_to(s_heart, s_img, LV_ALIGN_CENTER, 2, 34);
    lv_obj_set_hidden(s_heart, true);

    zzz_create(parent, x, y);
    apply_palette(s_tone);
    start_breathing();
    lv_timer_create(yawn_scheduler_cb, lv_rand(15000, 25000), NULL);
    return s_img;
}

void baby_set_skin_tone(uint8_t tone)
{
    if (tone >= BABY_SKIN_TONE_COUNT || tone == s_tone) return;
    s_tone = tone;
    apply_palette(tone);
}

uint8_t baby_get_skin_tone(void) { return s_tone; }

void baby_set_girl(bool girl)
{
    uint8_t d = girl ? DRAW_GIRL : DRAW_BOY;
    if (d == s_draw) return;
    s_draw = d;
    show_frame(s_cur); /* el mismo fotograma (tambien a mitad de bostezo) del otro dibujo */
}

void baby_set_awake(bool awake)
{
    if (awake == s_awake) return;
    s_awake = awake;
    if (!s_yawning) show_frame(rest_frame());
    zzz_set_visible(!awake);
}

void baby_set_heart_rate(uint16_t bpm)
{
    if (bpm == s_bpm) return;
    s_bpm = bpm;
    restart_heart_anim();
}

lv_obj_t *baby_widget_obj(void) { return s_img; }
