/* Corazon a la racha: al completar el primer «Coge mi mano» del dia (acompañar o demo) sale abajo
 * la semana de la racha (lunes a domingo, como en su pagina) con el hueco de hoy vacio, un corazon
 * vuela desde el dedo hasta ese hueco y, al llegar, el de hoy se enciende con un pequeño bote.
 * Ocupa la zona de la barra de navegacion, que no se ve mientras dura el final del gesto; se quita
 * al volver al bebe (ui_streak_fx_hide). Sin hora no se sabe que dia es hoy: no hace nada. */
#include <string.h>
#include <time.h>

#include "assets.h"
#include "net_wifi.h"
#include "ui_internal.h"
#include "ui_toy.h"

#define WEEK_N    7
#define BOX_X     12
#define BOX_Y     272
#define BOX_W     216
#define BOX_H     44
#define HEART_W   18  /* corazon de pixeles de 9x8 celdas de 2 px */
#define HEART_H   16
#define HEART_Y   6   /* dentro de la caja */
#define DAY_Y     25
#define STEP_X    29  /* 7 x 29 = 203 dentro de 216 */
#define FLY_DELAY 350
#define FLY_MS    900
#define POP_MS    420
#define FLY_SCALE_FROM 384 /* 1,5 x el corazon de 27 px que vuela */
#define FLY_SCALE_TO   (256 * HEART_W / 27)

static lv_obj_t *s_box, *s_week[WEEK_N], *s_day[WEEK_N], *s_fly;
static lv_obj_t *s_plus;    /* «+1» que va con el corazon y luego sube y se apaga */
static int32_t s_plus_y0;   /* y de partida de la subida */
static uint8_t s_hearts;    /* corazones que lleva el corazon que vuela */
static int s_today;
static int32_t s_from_x, s_from_y, s_to_x, s_to_y;

/* Corazon de la semana con el corazon de pixeles de la app: lleno coral, dorado (super), azul claro
 * (dia sin bebes) o vacio (contorno apagado); los dias que aun no han llegado, mas claros. La imagen
 * es de 9x8 y se estira sin suavizar al tamaño del objeto (multiplo de 9x8). */
void ui_week_heart_style(lv_obj_t *h, bool on, bool super, bool rest, bool future)
{
    toy_heart_t k = on && super ? TOY_HEART_SUPER : on ? TOY_HEART_FULL : rest ? TOY_HEART_REST : TOY_HEART_EMPTY;
    const lv_image_dsc_t *d = toy_heart_dsc(k, 1);
    if (lv_image_get_src(h) != d) lv_image_set_src(h, d);
    lv_image_set_inner_align(h, LV_IMAGE_ALIGN_STRETCH);
    lv_image_set_antialias(h, false);
    lv_obj_set_style_image_recolor_opa(h, LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_opa(h, future ? LV_OPA_50 : LV_OPA_COVER, 0);
}

static int32_t heart_x(int i) { return 6 + i * STEP_X + 1 + (28 - HEART_W) / 2; }

void ui_streak_fx_create(lv_obj_t *scr)
{
    s_box = lv_obj_create(scr);
    lv_obj_set_size(s_box, BOX_W, BOX_H);
    lv_obj_set_pos(s_box, BOX_X, BOX_Y);
    lv_obj_set_style_bg_color(s_box, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    toy_box(s_box, TOY_R_CARD, TOY_SHADOW_S);
    lv_obj_set_style_pad_all(s_box, 0, 0);
    lv_obj_set_scrollable(s_box, false);
    lv_obj_set_clickable(s_box, false);
    for (int i = 0; i < WEEK_N; i++) {
        lv_obj_t *h = s_week[i] = lv_image_create(s_box);
        ui_week_heart_style(h, false, false, false, false);
        lv_obj_set_size(h, HEART_W, HEART_H);
        lv_obj_set_pos(h, heart_x(i), HEART_Y);
        lv_obj_set_clickable(h, false);
        lv_obj_t *d = s_day[i] = lv_label_create(s_box);
        lv_label_set_text(d, "");
        lv_obj_set_style_text_font(d, &lv_font_es_12, 0);
        lv_obj_set_style_text_color(d, COL_MUTED, 0);
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(d, 28);
        lv_obj_set_pos(d, 6 + i * STEP_X, DAY_Y);
    }
    lv_obj_set_hidden(s_box, true);

    s_fly = toy_heart_img(scr, TOY_HEART_FULL, 3); /* 27x24 */
    lv_obj_set_hidden(s_fly, true);

    s_plus = lv_label_create(scr);
    lv_label_set_text(s_plus, "");
    lv_obj_set_style_text_font(s_plus, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(s_plus, COL_CORAL, 0);
    lv_obj_set_clickable(s_plus, false);
    lv_obj_set_hidden(s_plus, true);
}

/* ------------------------------------------------------------- animaciones */

static void box_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

/* el de hoy crece y vuelve (v 0..1000): 22 -> 30 -> 22 px, centrado */
static void pop_cb(void *var, int32_t v)
{
    lv_obj_t *h = var;
    int32_t up = v < 400 ? v * 1000 / 400 : (1000 - v) * 1000 / 600;
    int32_t w = HEART_W + 8 * up / 1000;
    int32_t hh = w * HEART_H / HEART_W;
    lv_obj_set_size(h, w, hh);
    lv_obj_set_pos(h, heart_x(s_today) - (w - HEART_W) / 2, HEART_Y - (hh - HEART_H) / 2);
}

/* vuelo (v 0..1000): en linea recta con una curva hacia arriba, encogiendo hasta su hueco */
static void fly_cb(void *var, int32_t v)
{
    lv_obj_t *o = var;
    int32_t x = s_from_x + (s_to_x - s_from_x) * v / 1000;
    int32_t arc = 60 * v * (1000 - v) / 250000; /* 0 -> 60 -> 0 px */
    int32_t y = s_from_y + (s_to_y - s_from_y) * v / 1000 - arc;
    lv_image_set_scale(o, (uint32_t)(FLY_SCALE_FROM + (FLY_SCALE_TO - FLY_SCALE_FROM) * v / 1000));
    lv_obj_set_pos(o, x - 13, y - 12); /* el pivote de la escala es el centro de la imagen de 27x24 */
    lv_obj_set_pos(s_plus, x + 12, y - 30); /* el «+1» va pegado al corazon, arriba a la derecha */
}

/* «+N» que sube 18 px y se apaga (v 0..1000) */
static void plus_cb(void *var, int32_t v)
{
    lv_obj_t *l = var;
    lv_obj_set_y(l, s_plus_y0 - 18 * v / 1000);
    lv_obj_set_style_text_opa(l, (lv_opa_t)(v < 500 ? 255 : 255 * (1000 - v) / 500), 0);
}

static void plus_done_cb(lv_anim_t *a) { lv_obj_set_hidden(a->var, true); }

static void plus_float(int32_t x, int32_t y)
{
    lv_anim_delete(s_plus, plus_cb);
    lv_obj_set_pos(s_plus, x, y);
    lv_obj_set_style_text_opa(s_plus, LV_OPA_COVER, 0);
    lv_obj_set_hidden(s_plus, false);
    lv_obj_move_foreground(s_plus);
    s_plus_y0 = y;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_plus);
    lv_anim_set_exec_cb(&a, plus_cb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 1100);
    lv_anim_set_completed_cb(&a, plus_done_cb);
    lv_anim_start(&a);
}

static void fly_start_cb(lv_anim_t *a)
{
    lv_obj_set_hidden(a->var, false);
    if (s_hearts) {
        lv_label_set_text_fmt(s_plus, "+%u", s_hearts);
        lv_obj_set_style_text_opa(s_plus, LV_OPA_COVER, 0);
        lv_obj_set_hidden(s_plus, false);
    }
}

static void fly_done_cb(lv_anim_t *a)
{
    lv_obj_set_hidden(a->var, true);
    if (s_hearts) plus_float(BOX_X + heart_x(s_today) + HEART_W - 4, BOX_Y - 20);
    lv_obj_t *h = s_week[s_today];
    ui_week_heart_style(h, true, false, false, false);
    lv_anim_t p;
    lv_anim_init(&p);
    lv_anim_set_var(&p, h);
    lv_anim_set_exec_cb(&p, pop_cb);
    lv_anim_set_values(&p, 0, 1000);
    lv_anim_set_duration(&p, POP_MS);
    lv_anim_start(&p);
}

/* ------------------------------------------------------------------- API */

void ui_streak_fx_hide(void)
{
    if (!s_box) return;
    lv_anim_delete(s_box, box_opa_cb);
    lv_anim_delete(s_fly, fly_cb);
    lv_anim_delete(s_plus, plus_cb);
    for (int i = 0; i < WEEK_N; i++) lv_anim_delete(s_week[i], pop_cb);
    lv_obj_set_hidden(s_box, true);
    lv_obj_set_hidden(s_fly, true);
    lv_obj_set_hidden(s_plus, true);
}

void ui_streak_fx_bonus(uint8_t n)
{
    if (!s_plus || !n) return;
    lv_label_set_text_fmt(s_plus, "+%u", n);
    /* encima del hueco de hoy si la semana se ve; si no, encima del bebe */
    if (!lv_obj_is_hidden(s_box)) plus_float(BOX_X + heart_x(s_today) + HEART_W - 4, BOX_Y - 20);
    else plus_float(108, 112);
}

void ui_streak_fx_play(uint8_t week, uint8_t super, uint8_t rest, int32_t x, int32_t y, uint8_t hearts)
{
    if (!s_box || !net_wifi_time_valid()) return;
    ui_streak_fx_hide();
    s_hearts = hearts;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    s_today = (tm.tm_wday + 6) % 7; /* 0 = lunes */
    const char *ini = tr(STR_WEEKDAY_INITIALS); /* 7 letras empezando en domingo */
    bool ini_ok = strlen(ini) == 7;
    for (int i = 0; i < WEEK_N; i++) {
        int off = s_today - i; /* dias atras; < 0 = aun no ha llegado */
        bool past = off > 0;   /* hoy aun sin encender: lo enciende el corazon al llegar */
        bool on = past && (week & (1u << off));
        bool sup = on && (super & (1u << off));
        bool rst = past && !on && (rest & (1u << off));
        ui_week_heart_style(s_week[i], on, sup, rst, off < 0);
        lv_obj_set_size(s_week[i], HEART_W, HEART_H);
        lv_obj_set_pos(s_week[i], heart_x(i), HEART_Y);
        char d[2] = { ini_ok ? ini[(i + 1) % 7] : '\0', '\0' };
        lv_label_set_text(s_day[i], d);
        lv_obj_set_style_text_color(s_day[i], off == 0 ? COL_CORAL_INK : COL_MUTED, 0);
    }

    lv_obj_set_hidden(s_box, false);
    lv_obj_set_style_opa(s_box, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_box);
    lv_anim_set_exec_cb(&a, box_opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, 250);
    lv_anim_start(&a);

    s_from_x = x;
    s_from_y = y;
    s_to_x = BOX_X + heart_x(s_today) + HEART_W / 2;
    s_to_y = BOX_Y + HEART_Y + HEART_H / 2;
    fly_cb(s_fly, 0);
    lv_anim_t f;
    lv_anim_init(&f);
    lv_anim_set_var(&f, s_fly);
    lv_anim_set_exec_cb(&f, fly_cb);
    lv_anim_set_values(&f, 0, 1000);
    lv_anim_set_delay(&f, FLY_DELAY);
    lv_anim_set_duration(&f, FLY_MS);
    lv_anim_set_path_cb(&f, lv_anim_path_ease_in_out);
    lv_anim_set_start_cb(&f, fly_start_cb);
    lv_anim_set_completed_cb(&f, fly_done_cb);
    lv_anim_start(&f);
}
