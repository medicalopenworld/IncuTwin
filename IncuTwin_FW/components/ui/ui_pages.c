#include "ui_pages.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "esp_log.h"
#include "net_wifi.h"
#include "ui_internal.h"

static const char *TAG = "pages";

/* Zona que tapan las paginas: entre la barra superior y la barra de estado */
#define PG_Y      38
#define PG_H      240
#define DOTS_Y    280
#define SLIDE_MS  260

/* Datos simulados de la demo (Camerun). Cambiar aqui el hospital o los logros. */
#define DEMO_COUNTRY        "CM"
#define DEMO_HOSPITAL       "Hospital de Yaundé"
#define DEMO_UTC_OFFSET_MIN 60 /* Camerun: UTC+1, sin horario de verano */
#define DEMO_HANDS_BASE     37
#define DEMO_FAMILIES       12
#define DEMO_DAYS           14
#define DEMO_WEIGHT_G       420

static lv_obj_t *s_pg[UI_PAGES_COUNT]; /* s_pg[0] no existe: la pagina 0 es la pantalla */
static lv_obj_t *s_dots[UI_PAGES_COUNT];
static lv_obj_t *s_flag, *s_lbl_country, *s_lbl_hosp_t, *s_lbl_hosp, *s_lbl_time_t, *s_lbl_time;
static lv_obj_t *s_lbl_ach_t, *s_lbl_ach_sub;
static lv_obj_t *s_tile_num[4], *s_tile_cap[4];
static int s_page;
static bool s_enabled;
static unsigned s_hands; /* gestos completados en esta demo */
static char s_country[TWIN_COUNTRY_LEN] = DEMO_COUNTRY;

/* ------------------------------------------------------------- helpers */

static lv_obj_t *panel(lv_obj_t *scr)
{
    lv_obj_t *p = lv_obj_create(scr);
    lv_obj_set_size(p, 240, PG_H);
    lv_obj_set_pos(p, 240, PG_Y);
    lv_obj_set_style_bg_color(p, COL_BG, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_scrollable(p, false); /* si no, el desplazamiento se come el gesto */
    lv_obj_set_hidden(p, true);
    return p; /* pulsable (por defecto): tapa los toques al bebe que queda debajo */
}

static lv_obj_t *card(lv_obj_t *parent, int32_t y, int32_t h)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, 216, h);
    lv_obj_set_pos(c, 12, y);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    return c;
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, lv_color_t col, int32_t x, int32_t y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void clock_refresh(void)
{
    if (!net_wifi_time_valid()) {
        lv_label_set_text(s_lbl_time, "--:--");
        return;
    }
    time_t t = time(NULL) + (time_t)DEMO_UTC_OFFSET_MIN * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    if (strcmp(lv_label_get_text(s_lbl_time), buf) != 0) lv_label_set_text(s_lbl_time, buf);
}

static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_enabled && s_page == 1) clock_refresh();
}

static void hands_refresh(void) { lv_label_set_text_fmt(s_tile_num[0], "%u", DEMO_HANDS_BASE + s_hands); }

static void dots_refresh(void)
{
    /* el punto de la pagina actual es una pildora de 14 px; los otros, puntos de 6 */
    int32_t x = 120 - (14 + 6 * (UI_PAGES_COUNT - 1) + 6 * (UI_PAGES_COUNT - 1)) / 2;
    for (int i = 0; i < UI_PAGES_COUNT; i++) {
        int32_t w = i == s_page ? 14 : 6;
        lv_obj_set_hidden(s_dots[i], !s_enabled);
        lv_obj_set_style_bg_color(s_dots[i], i == s_page ? COL_CORAL : COL_OFFLINE, 0);
        lv_obj_set_size(s_dots[i], w, 6);
        lv_obj_set_pos(s_dots[i], x, DOTS_Y);
        x += w + 6;
    }
}

static void anim_x_cb(void *var, int32_t v) { lv_obj_set_x((lv_obj_t *)var, v); }

/* Al acabar el deslizamiento se esconde el panel que ha quedado fuera de pantalla. Se decide
 * por la pagina actual y no por lv_obj_get_x(): las coordenadas solo se actualizan en el
 * siguiente refresco de layout y aqui aun valdrian lo del fotograma anterior (nunca 0). */
static void anim_done_cb(lv_anim_t *a)
{
    lv_obj_t *p = a->var;
    int idx = (int)(intptr_t)lv_obj_get_user_data(p);
    if (idx != s_page) lv_obj_set_hidden(p, true);
}

/* Cada panel i se coloca en (i - pagina) * 240: el actual en 0, los demas a los lados */
static void slide_to(int page, bool animate)
{
    s_page = page;
    for (int i = 1; i < UI_PAGES_COUNT; i++) {
        int32_t to = (i - page) * 240;
        lv_anim_delete(s_pg[i], anim_x_cb);
        if (!animate) {
            lv_obj_set_x(s_pg[i], to);
            lv_obj_set_hidden(s_pg[i], to != 0);
            continue;
        }
        int32_t from = lv_obj_get_style_x(s_pg[i], LV_PART_MAIN); /* la x pedida, no la pintada */
        if (from == to) continue;
        lv_obj_set_hidden(s_pg[i], false);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_pg[i]);
        lv_anim_set_exec_cb(&a, anim_x_cb);
        lv_anim_set_values(&a, from, to);
        lv_anim_set_duration(&a, SLIDE_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a, anim_done_cb);
        lv_anim_start(&a);
    }
    if (page == 1) clock_refresh();
    dots_refresh();
}

/* ----------------------------------------------------------------- build */

void ui_pages_create(lv_obj_t *scr)
{
    /* pagina 1: pais, hospital y hora local */
    lv_obj_t *p = s_pg[1] = panel(scr);
    lv_obj_set_user_data(p, (void *)(intptr_t)1);
    s_flag = lv_image_create(p);
    lv_image_set_src(s_flag, &img_flag_cm);
    lv_obj_align(s_flag, LV_ALIGN_TOP_MID, 0, 10);
    s_lbl_country = text(p, &lv_font_es_20, COL_NAVY_DARK, 0, 0);
    lv_obj_align(s_lbl_country, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *c = card(p, 112, 52);
    s_lbl_hosp_t = text(c, &lv_font_es_12, COL_OFFLINE, 12, 7);
    s_lbl_hosp = text(c, &lv_font_es_16, COL_NAVY_DARK, 12, 24);
    lv_obj_set_width(s_lbl_hosp, 192);
    lv_label_set_long_mode(s_lbl_hosp, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(s_lbl_hosp, DEMO_HOSPITAL);

    c = card(p, 172, 56);
    s_lbl_time_t = text(c, &lv_font_es_12, COL_OFFLINE, 12, 7);
    s_lbl_time = text(c, &lv_font_es_20, COL_CORAL, 12, 25);
    lv_label_set_text(s_lbl_time, "--:--");
    lv_obj_t *utc = text(c, &lv_font_es_12, COL_OFFLINE, 0, 0);
    lv_label_set_text_fmt(utc, "UTC%+d", DEMO_UTC_OFFSET_MIN / 60);
    lv_obj_align(utc, LV_ALIGN_BOTTOM_RIGHT, -12, -9);

    /* pagina 2: logros acumulados */
    p = s_pg[2] = panel(scr);
    lv_obj_set_user_data(p, (void *)(intptr_t)2);
    s_lbl_ach_t = text(p, &lv_font_es_20, COL_NAVY_DARK, 0, 0);
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, 8);
    s_lbl_ach_sub = text(p, &lv_font_es_12, COL_OFFLINE, 0, 0);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, 36);
    for (int i = 0; i < 4; i++) {
        c = card(p, 60 + (i / 2) * 88, 80);
        lv_obj_set_size(c, 104, 80);
        lv_obj_set_x(c, 12 + (i % 2) * 112);
        s_tile_num[i] = text(c, &lv_font_es_20, COL_CORAL, 0, 0);
        lv_obj_align(s_tile_num[i], LV_ALIGN_TOP_MID, 0, 12);
        s_tile_cap[i] = text(c, &lv_font_es_12, COL_NAVY_DARK, 0, 0);
        lv_obj_set_width(s_tile_cap[i], 96);
        lv_obj_set_style_text_align(s_tile_cap[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_tile_cap[i], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align(s_tile_cap[i], LV_ALIGN_TOP_MID, 0, 40);
    }
    hands_refresh();
    lv_label_set_text_fmt(s_tile_num[1], "%u", DEMO_FAMILIES);
    lv_label_set_text_fmt(s_tile_num[2], "%u", DEMO_DAYS);
    lv_label_set_text_fmt(s_tile_num[3], "+%u g", DEMO_WEIGHT_G);

    /* puntos de pagina */
    for (int i = 0; i < UI_PAGES_COUNT; i++) {
        s_dots[i] = lv_obj_create(scr);
        lv_obj_set_size(s_dots[i], 6, 6);
        lv_obj_set_style_radius(s_dots[i], 3, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_set_scrollable(s_dots[i], false);
        lv_obj_set_clickable(s_dots[i], false);
    }
    lv_timer_create(clock_timer_cb, 5000, NULL);
    ui_pages_retranslate();
    slide_to(0, false);
}

void ui_pages_retranslate(void)
{
    lv_label_set_text(s_lbl_country, tr_country(s_country));
    lv_label_set_text(s_lbl_hosp_t, tr(STR_PG_HOSPITAL));
    lv_label_set_text(s_lbl_time_t, tr(STR_PG_LOCAL_TIME));
    lv_label_set_text(s_lbl_ach_t, tr(STR_PG_ACHIEVEMENTS));
    lv_label_set_text(s_lbl_ach_sub, tr(STR_PG_ACH_SUB));
    static const ui_str_t CAP[4] = { STR_PG_HANDS, STR_PG_FAMILIES, STR_PG_DAYS, STR_PG_WEIGHT };
    for (int i = 0; i < 4; i++) lv_label_set_text(s_tile_cap[i], tr(CAP[i]));
    lv_obj_align(s_lbl_country, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, 36);
}

/* ------------------------------------------------------------------- API */

void ui_pages_apply(const twin_snapshot_t *s)
{
    const char *cc = s->inc.country[0] ? s->inc.country : DEMO_COUNTRY;
    if (strcmp(cc, s_country) != 0) {
        memcpy(s_country, cc, TWIN_COUNTRY_LEN);
        lv_obj_set_hidden(s_flag, strcmp(s_country, "CM") != 0); /* solo hay bandera de Camerun */
        ui_pages_retranslate();
    }
    if (s->demo == s_enabled) return;
    s_enabled = s->demo;
    if (!s_enabled) { /* los logros de la demo empiezan de cero cada vez */
        s_hands = 0;
        hands_refresh();
    }
    slide_to(0, false);
}

bool ui_pages_swipe(int dir)
{
    if (!s_enabled) return false;
    int to = s_page + (dir < 0 ? 1 : -1);
    if (to < 0 || to >= UI_PAGES_COUNT) return false;
    ESP_LOGI(TAG, "pagina %d -> %d", s_page + 1, to + 1);
    slide_to(to, true);
    return true;
}

int ui_pages_current(void) { return s_page; }

void ui_pages_note_hand(void)
{
    if (!s_enabled) return;
    s_hands++;
    hands_refresh();
}
