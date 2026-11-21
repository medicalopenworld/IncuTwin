#include "ui_pages.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "net_wifi.h"
#include "settings.h"
#include "twin_geo.h"
#include "world_map.h"
#include "ui_internal.h"

static const char *TAG = "pages";

/* Zona que tapan las paginas: entre la barra superior y la barra de estado */
#define PG_Y      38
#define PG_H      240
#define DOTS_Y    280
#define SLIDE_MS  260

/* Mapa a toda la pagina, pintado en el panel (components/world_map) con el zoom justo para ver
 * tu IncuTwin y la IncuNest; arriba la etiqueta del pais y abajo la tarjeta de distancia y hora,
 * encima del mapa. Los puntos se colocan entre las dos. */
#define MAP_W       240
#define MAP_H       PG_H
#define CHIP_H      40  /* bandera + nombre */
#define CHIP_H_DEMO 58  /* + hospital */
#define INFO_H      58  /* distancia + hora */
#define COL_LAND    lv_color_hex(0xEEE8D8)
#define COL_SEA     lv_color_hex(0xCFE2F3)

/* Datos simulados de la demo (Camerun). Cambiar aqui el hospital o los logros. */
#define DEMO_COUNTRY        "CM"
#define DEMO_HOSPITAL       "Hospital de Yaundé"
#define DEMO_MY_DAYS        22 /* dias que has acompañado (antes de hoy) */
#define DEMO_STREAK         4  /* racha de dias seguidos (hasta ayer) */
#define DEMO_FAMILIES       12 /* otras familias que tambien le acompañan */
#define DEMO_BABY_DAYS      14 /* dias del bebe en esta IncuNest */

static lv_obj_t *s_pg[UI_PAGES_COUNT]; /* s_pg[0] no existe: la pagina 0 es la pantalla */
static lv_obj_t *s_dots[UI_PAGES_COUNT];
static lv_obj_t *s_canvas, *s_chip, *s_flag, *s_lbl_country, *s_lbl_hosp;
static lv_obj_t *s_lbl_dist, *s_lbl_time_t, *s_lbl_time, *s_lbl_utc;
static lv_obj_t *s_dot_home, *s_dot_inc, *s_ping, *s_route;
static uint16_t *s_map_buf; /* 240x240 RGB565 en PSRAM */
static lv_point_precise_t s_route_pts[2];
static int s_avail = 1;    /* paginas que se pueden ver: 1 (solo bebe), 2 (+ pais) o 3 (demo) */
static int16_t s_utc_min;  /* hora local del pais de la IncuNest */
static lv_obj_t *s_lbl_ach_t, *s_lbl_ach_sub;
static lv_obj_t *s_tile_num[4], *s_tile_cap[4];
static int s_page;
static bool s_enabled;
static unsigned s_hands; /* gestos completados en esta demo: hoy cuenta como dia acompañado */
static char s_country[TWIN_COUNTRY_LEN] = ""; /* pais pintado; "" = ninguno todavia */

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
    time_t t = time(NULL) + (time_t)s_utc_min * 60;
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    if (strcmp(lv_label_get_text(s_lbl_time), buf) != 0) lv_label_set_text(s_lbl_time, buf);
}

static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_page == 1) clock_refresh();
}

/* Completar un "Coge mi mano" hoy suma hoy a los dias acompañados y alarga la racha */
static void hands_refresh(void)
{
    unsigned today = s_hands > 0 ? 1 : 0;
    lv_label_set_text_fmt(s_tile_num[0], "%u", DEMO_MY_DAYS + today);
    lv_label_set_text_fmt(s_tile_num[1], "%u", DEMO_STREAK + today);
}

static void dots_refresh(void)
{
    /* el punto de la pagina actual es una pildora de 14 px; los otros, puntos de 6 */
    int32_t x = 120 - (14 + 6 * (s_avail - 1) + 6 * (s_avail - 1)) / 2;
    for (int i = 0; i < UI_PAGES_COUNT; i++) {
        int32_t w = i == s_page ? 14 : 6;
        lv_obj_set_hidden(s_dots[i], i >= s_avail || s_avail < 2);
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

/* Circulo de d px con borde blanco de `border` px (se centra con dot_at) */
static lv_obj_t *dot(lv_obj_t *parent, int32_t d, lv_color_t col, int32_t border)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, lv_color_white(), 0);
    lv_obj_set_style_border_width(o, border, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    return o;
}

static void dot_at(lv_obj_t *o, int32_t x, int32_t y)
{
    int32_t d = lv_obj_get_style_width(o, LV_PART_MAIN);
    lv_obj_set_pos(o, x - d / 2, y - d / 2);
}

static int32_t s_inc_x, s_inc_y; /* centro de la IncuNest en el mapa */

/* Onda que sale de la IncuNest en el mapa: crece y se apaga (v 0..1000) */
static void ping_cb(void *var, int32_t v)
{
    lv_obj_t *o = var;
    int32_t d = 10 + 16 * v / 1000;
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, s_inc_x - d / 2, s_inc_y - d / 2);
    lv_obj_set_style_bg_opa(o, (lv_opa_t)(140 * (1000 - v) / 1000), 0);
}

/* "3.640" (es) / "3,640" (en), redondeado a 10 km */
static void fmt_km(char *buf, size_t n, uint32_t km)
{
    km = (km + 5) / 10 * 10;
    char sep = settings_lang() == SETTINGS_LANG_EN ? ',' : '.';
    if (km >= 1000) snprintf(buf, n, "%u%c%03u", (unsigned)(km / 1000), sep, (unsigned)(km % 1000));
    else snprintf(buf, n, "%u", (unsigned)km);
}

/* Tarjeta blanca semitransparente encima del mapa */
static lv_obj_t *overlay(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_90, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_shadow_width(c, 8, 0);
    lv_obj_set_style_shadow_opa(c, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    return c;
}

/* Pagina del pais para s_country (ya comprobado contra la tabla de twin_geo) */
static void country_page_refresh(void)
{
    const twin_geo_t *g = twin_geo_find(s_country);
    if (!g) return;
    s_utc_min = g->utc_min;

    /* etiqueta: bandera + nombre (+ hospital en la demo) */
    const lv_image_dsc_t *flag = flag_image(s_country);
    lv_obj_set_hidden(s_flag, flag == NULL);
    if (flag) lv_image_set_src(s_flag, flag);
    lv_label_set_text(s_lbl_country, tr_country(s_country));
    lv_obj_set_hidden(s_lbl_hosp, !s_enabled);
    int32_t chip_h = s_enabled ? CHIP_H_DEMO : CHIP_H;
    lv_obj_set_height(s_chip, chip_h);

    /* mapa: encuadre para los dos puntos entre la etiqueta y la tarjeta de abajo */
    if (s_map_buf) {
        const wm_frame_t f = { .w = MAP_W, .h = MAP_H, .top = (int16_t)(chip_h + 8),
                               .bottom = INFO_H + 8, .margin = 12, .min_ppd = 1.2f, .max_ppd = 9.0f };
        wm_view_t v;
        wm_fit(&v, &f, TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, g->lat, g->lon);
        int64_t t0 = esp_timer_get_time();
        wm_render(s_map_buf, MAP_W, MAP_H, &v, lv_color_to_u16(COL_LAND), lv_color_to_u16(COL_SEA));
        ESP_LOGI(TAG, "mapa %s: %.1f px/grado, %lld ms", s_country, (double)v.sy,
                 (long long)((esp_timer_get_time() - t0) / 1000));
        lv_obj_invalidate(s_canvas);
        float hx, hy, ix, iy;
        wm_project(&v, TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, &hx, &hy);
        wm_project(&v, g->lat, g->lon, &ix, &iy);
        s_inc_x = (int32_t)ix;
        s_inc_y = (int32_t)iy;
        s_route_pts[0] = (lv_point_precise_t){ (int32_t)hx, (int32_t)hy };
        s_route_pts[1] = (lv_point_precise_t){ s_inc_x, s_inc_y };
        lv_line_set_points(s_route, s_route_pts, 2);
        dot_at(s_dot_home, (int32_t)hx, (int32_t)hy);
        dot_at(s_dot_inc, s_inc_x, s_inc_y);
    }

    char km[16], buf[48];
    fmt_km(km, sizeof(km), twin_geo_distance_km(TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, g->lat, g->lon));
    snprintf(buf, sizeof(buf), tr(STR_PG_DISTANCE), km);
    lv_label_set_text(s_lbl_dist, buf);
    int h = g->utc_min / 60, m = g->utc_min % 60;
    if (m) lv_label_set_text_fmt(s_lbl_utc, "UTC%+d:%02d", h, m < 0 ? -m : m);
    else lv_label_set_text_fmt(s_lbl_utc, "UTC%+d", h);
    lv_obj_align(s_lbl_utc, LV_ALIGN_BOTTOM_RIGHT, -12, -9);
    clock_refresh();
}

/* ----------------------------------------------------------------- build */

void ui_pages_create(lv_obj_t *scr)
{
    /* pagina 1: mapa (tu IncuTwin -> la IncuNest) con pais, distancia y hora encima */
    lv_obj_t *p = s_pg[1] = panel(scr);
    lv_obj_set_user_data(p, (void *)(intptr_t)1);
    lv_obj_set_style_bg_color(p, COL_SEA, 0);
    s_canvas = lv_canvas_create(p);
    lv_obj_set_pos(s_canvas, 0, 0);
    s_map_buf = heap_caps_malloc((size_t)MAP_W * MAP_H * 2, MALLOC_CAP_SPIRAM);
    if (s_map_buf) {
        memset(s_map_buf, 0, (size_t)MAP_W * MAP_H * 2);
        lv_canvas_set_buffer(s_canvas, s_map_buf, MAP_W, MAP_H, LV_COLOR_FORMAT_RGB565);
    } else {
        ESP_LOGE(TAG, "sin PSRAM para el mapa");
    }
    s_route = lv_line_create(p);
    lv_obj_set_pos(s_route, 0, 0);
    lv_obj_set_style_line_width(s_route, 3, 0);
    lv_obj_set_style_line_color(s_route, COL_CORAL, 0);
    lv_obj_set_style_line_rounded(s_route, true, 0);
    lv_obj_set_style_line_dash_width(s_route, 6, 0);
    lv_obj_set_style_line_dash_gap(s_route, 4, 0);
    s_ping = dot(p, 12, COL_CORAL, 0);
    s_dot_home = dot(p, 14, COL_NAVY, 3);
    s_dot_inc = dot(p, 14, COL_CORAL, 3);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_ping);
    lv_anim_set_exec_cb(&a, ping_cb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 1600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* arriba a la izquierda: bandera + pais (+ hospital en la demo) */
    s_chip = overlay(p);
    lv_obj_set_pos(s_chip, 8, 8);
    lv_obj_set_width(s_chip, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(s_chip, 224, 0);
    lv_obj_set_style_pad_hor(s_chip, 8, 0);
    lv_obj_set_style_pad_ver(s_chip, 7, 0);
    lv_obj_set_style_pad_row(s_chip, 2, 0);
    lv_obj_set_flex_flow(s_chip, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *row = lv_obj_create(s_chip);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, false);
    s_flag = lv_image_create(row);
    s_lbl_country = lv_label_create(row);
    lv_obj_set_style_text_font(s_lbl_country, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(s_lbl_country, COL_NAVY_DARK, 0);
    lv_obj_set_style_max_width(s_lbl_country, 150, 0);
    lv_label_set_long_mode(s_lbl_country, LV_LABEL_LONG_MODE_DOTS);
    s_lbl_hosp = lv_label_create(s_chip);
    lv_obj_set_style_text_font(s_lbl_hosp, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_hosp, COL_NAVY_DARK, 0);
    lv_label_set_text(s_lbl_hosp, DEMO_HOSPITAL);

    /* abajo: distancia y hora local */
    lv_obj_t *c = overlay(p);
    lv_obj_set_size(c, 224, INFO_H);
    lv_obj_set_pos(c, 8, PG_H - INFO_H - 6);
    s_lbl_dist = text(c, &lv_font_es_14, COL_NAVY_DARK, 12, 6);
    s_lbl_time_t = text(c, &lv_font_es_12, COL_OFFLINE, 12, 33);
    s_lbl_time = text(c, &lv_font_es_20, COL_CORAL, 86, 28);
    lv_label_set_text(s_lbl_time, "--:--");
    s_lbl_utc = text(c, &lv_font_es_12, COL_OFFLINE, 0, 0);

    /* pagina 2: logros acumulados */
    p = s_pg[2] = panel(scr);
    lv_obj_set_user_data(p, (void *)(intptr_t)2);
    s_lbl_ach_t = text(p, &lv_font_es_20, COL_NAVY_DARK, 0, 0);
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, 8);
    s_lbl_ach_sub = text(p, &lv_font_es_12, COL_OFFLINE, 0, 0);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, 36);
    for (int i = 0; i < 4; i++) {
        c = card(p, 58 + (i / 2) * 92, 88); /* hasta 3 lineas de texto */
        lv_obj_set_size(c, 104, 88);
        lv_obj_set_x(c, 12 + (i % 2) * 112);
        s_tile_num[i] = text(c, &lv_font_es_20, COL_CORAL, 0, 0);
        lv_obj_align(s_tile_num[i], LV_ALIGN_TOP_MID, 0, 10);
        s_tile_cap[i] = text(c, &lv_font_es_12, COL_NAVY_DARK, 0, 0);
        lv_obj_set_width(s_tile_cap[i], 96);
        lv_obj_set_style_text_align(s_tile_cap[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_tile_cap[i], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align(s_tile_cap[i], LV_ALIGN_TOP_MID, 0, 36);
    }
    hands_refresh();
    lv_label_set_text_fmt(s_tile_num[2], "+%u", DEMO_FAMILIES);
    lv_label_set_text_fmt(s_tile_num[3], "%u", DEMO_BABY_DAYS);

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
    lv_label_set_text(s_lbl_time_t, tr(STR_PG_LOCAL_TIME));
    country_page_refresh();
    lv_label_set_text(s_lbl_ach_t, tr(STR_PG_ACHIEVEMENTS));
    lv_label_set_text(s_lbl_ach_sub, tr(STR_PG_ACH_SUB));
    static const ui_str_t CAP[4] = { STR_PG_MY_DAYS, STR_PG_STREAK, STR_PG_FAMILIES, STR_PG_DAYS };
    for (int i = 0; i < 4; i++) lv_label_set_text(s_tile_cap[i], tr(CAP[i]));
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, 36);
}

/* ------------------------------------------------------------------- API */

void ui_pages_apply(const twin_snapshot_t *s)
{
    bool demo_changed = s->demo != s_enabled;
    s_enabled = s->demo;
    if (demo_changed && !s_enabled) { /* los logros de la demo empiezan de cero cada vez */
        s_hands = 0;
        hands_refresh();
    }
    /* pais: el del estado; en la demo, Camerun si el escenario no trae otro */
    const char *cc = s->inc.country[0] ? s->inc.country : (s_enabled ? DEMO_COUNTRY : "");
    bool known = twin_geo_find(cc) != NULL;
    if (known && (strcmp(cc, s_country) != 0 || demo_changed)) {
        memcpy(s_country, cc, TWIN_COUNTRY_LEN);
        country_page_refresh();
    }
    /* fuera de la demo: bebe + pais (si se sabe); los logros llegaran con hmi-stay-history */
    int avail = s_enabled ? UI_PAGES_COUNT : known ? 2 : 1;
    if (avail != s_avail || demo_changed) {
        s_avail = avail;
        slide_to(s_page < avail && !demo_changed ? s_page : 0, false);
    }
}

bool ui_pages_swipe(int dir)
{
    int to = s_page + (dir < 0 ? 1 : -1);
    if (to < 0 || to >= s_avail) return false;
    ESP_LOGI(TAG, "pagina %d -> %d", s_page + 1, to + 1);
    slide_to(to, true);
    return true;
}

bool ui_pages_goto(int page)
{
    if (page < 0 || page >= s_avail || page == s_page) return false;
    ESP_LOGI(TAG, "pagina %d -> %d (toque)", s_page + 1, page + 1);
    slide_to(page, true);
    return true;
}

int ui_pages_current(void) { return s_page; }

void ui_pages_note_hand(void)
{
    if (!s_enabled) return;
    s_hands++;
    hands_refresh();
}
