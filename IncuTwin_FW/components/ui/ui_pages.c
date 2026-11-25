#include "ui_pages.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "acc_logic.h"
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

/* Zona que tapan las paginas: empieza donde el bebe (BABY_Y de ui_home, para taparlo entero,
 * gorro incluido) y acaba en la barra de estado (y 257). Su franja de arriba (PG_TOP) queda bajo
 * la barra del WiFi: sin contenido. Los puntos de pagina van en el margen de abajo. Antes: PG_Y 38,
 * PG_H 240, DOTS_Y 280 (por debajo quedaba la barra de estado en 288). */
#define PG_Y      14
#define PG_H      243
#define PG_TOP    22
#define DOTS_Y    250 /* entre la fila de Inicio (211..248) y la barra de estado (257) */
#define SLIDE_MS  260
/* logros, dentro de su caja (que empieza en PG_TOP): antes titulo 8 y subtitulo 36 */
#define ACH_T_Y   4
#define ACH_SUB_Y 28

/* Mapa a toda la pagina, pintado en el panel (components/world_map) con el zoom justo para ver
 * tu IncuTwin y la IncuNest; arriba la etiqueta del pais y los km sobre la linea que los une.
 * La hora del pais va en la barra de estado (ui_pages_status_text). */
#define MAP_W       240
#define MAP_H       PG_H
#define CHIP_H      40  /* bandera + nombre */
#define CHIP_H_DEMO 58  /* + hospital */
#define MAP_BOTTOM  16  /* margen de abajo del encuadre (los puntos de pagina) */
#define MAP_TOP     PG_TOP /* el mapa llega hasta arriba, pero el encuadre empieza bajo la barra */
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
static lv_obj_t *s_km, *s_lbl_km; /* pastilla con los km, en el centro de la linea */
static lv_obj_t *s_dot_home, *s_dot_inc, *s_ping, *s_route;
static uint16_t *s_map_buf; /* 240x240 RGB565 en PSRAM */
static lv_point_precise_t s_route_pts[2];
static int s_avail = 1;    /* paginas: 1 (solo bebe), 2 (+ pais), 3 (+ logros o racha) */
static int16_t s_utc_min;  /* hora local del pais de la IncuNest */
static char s_clock[8];    /* "HH:MM" del pais pintado en la barra; "" = sin hora */
static lv_obj_t *s_ach_box, *s_lbl_ach_t, *s_lbl_ach_sub;
static lv_obj_t *s_tile_num[4], *s_tile_cap[4];
static int s_page;
static bool s_enabled;
static unsigned s_hands; /* gestos completados en esta demo: hoy cuenta como dia acompañado */
static char s_country[TWIN_COUNTRY_LEN] = ""; /* pais pintado; "" = ninguno todavia */
/* Modo acompañar: pagina 2 = la racha. 7 corazones (hoy a la derecha) con la inicial del dia
 * debajo y una frase. En la demo, la pagina 2 son los logros. */
#define WEEK_N     7
static lv_obj_t *s_streak_box, *s_lbl_streak_t, *s_week[WEEK_N], *s_week_day[WEEK_N], *s_lbl_streak;
static bool s_acc_on;
static int s_week_mask = -1; /* pintado; -1 = forzar */
static uint8_t s_super_mask;  /* super corazones, mismos bits */
static uint8_t s_rest_mask;   /* dias sin bebes (no rompen la racha), mismos bits */
static int32_t s_week_day_drawn = -1; /* dia (tm_yday) de las iniciales pintadas */

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

/* Hora del pais de la IncuNest; si cambia y se esta viendo el mapa, la barra de estado se
 * vuelve a pintar (la pinta ui_home con ui_pages_status_text). */
static void clock_refresh(void)
{
    char buf[sizeof(s_clock)] = "";
    if (net_wifi_time_valid()) {
        time_t t = time(NULL) + (time_t)s_utc_min * 60;
        struct tm tm;
        gmtime_r(&t, &tm);
        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    }
    if (strcmp(buf, s_clock) == 0) return;
    memcpy(s_clock, buf, sizeof(s_clock));
    if (s_page == 1) ui_home_status_refresh();
}

static void streak_days_refresh(void);

static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    clock_refresh();
    if (s_page == 2 && s_acc_on) streak_days_refresh(); /* las iniciales cambian a medianoche */
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
    if (page == 2 && s_acc_on) streak_days_refresh();
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

static void chip_clicked(lv_event_t *e)
{
    (void)e;
    ui_home_goto_baby();
}

/* La racha, semana de lunes (izquierda) a domingo: se rellena de izquierda a derecha segun pasan
 * los dias. Las mascaras son de los ultimos 7 dias (bit 0 = hoy), que cubren la semana en curso.
 * Corazon lleno = dia con "Coge mi mano"; dorado = super corazon (3 min); azul claro = dia sin
 * bebes (no rompe la racha); los dias que aun no han llegado, casi transparentes. Debajo, la inicial del dia (hoy en coral) y una frase. */
static void streak_draw(void)
{
    bool valid = net_wifi_time_valid();
    struct tm tm = { 0 };
    if (valid) {
        time_t t = time(NULL);
        localtime_r(&t, &tm);
    }
    s_week_day_drawn = valid ? tm.tm_yday : -2;
    uint8_t mask = s_week_mask < 0 ? 0 : (uint8_t)s_week_mask;
    int today = (tm.tm_wday + 6) % 7; /* 0 = lunes */
    const char *ini = tr(STR_WEEKDAY_INITIALS); /* 7 letras empezando en domingo */
    for (int i = 0; i < WEEK_N; i++) {
        int off = today - i; /* dias atras; < 0 = aun no ha llegado */
        bool on = valid && off >= 0 && (mask & (1u << off));
        bool super = on && (s_super_mask & (1u << off));
        bool rest = valid && !on && off >= 0 && (s_rest_mask & (1u << off));
        lv_obj_t *h = s_week[i];
        lv_obj_set_style_image_recolor(h, super ? COL_SUPER : rest ? COL_REST : COL_OFFLINE, 0);
        lv_obj_set_style_image_recolor_opa(h, on && !super ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        lv_obj_set_style_image_opa(h, on || rest ? LV_OPA_COVER : (valid && off < 0) ? LV_OPA_20 : LV_OPA_40, 0);
        char s[2] = { valid && strlen(ini) == 7 ? ini[(i + 1) % 7] : '\0', '\0' };
        lv_label_set_text(s_week_day[i], s);
        lv_obj_set_style_text_color(s_week_day[i], valid && off == 0 ? COL_CORAL : COL_OFFLINE, 0);
    }

    int n = 0;
    ui_str_t id = STR_STREAK_NO_TIME;
    if (valid && (s_super_mask & 1u)) {
        id = STR_STREAK_SUPER;
    } else if (valid) {
        static const ui_str_t BY_MSG[] = {
            [ACC_SM_NONE] = STR_STREAK_NONE,     [ACC_SM_START] = STR_STREAK_START,
            [ACC_SM_TODAY1] = STR_STREAK_TODAY1, [ACC_SM_RUN] = STR_STREAK_RUN,
            [ACC_SM_WEEK] = STR_STREAK_WEEK,     [ACC_SM_KEEP1] = STR_STREAK_KEEP1,
            [ACC_SM_KEEP] = STR_STREAK_KEEP,     [ACC_SM_REST] = STR_STREAK_REST,
        };
        id = BY_MSG[acc_streak_msg(mask, s_rest_mask, &n)];
    }
    lv_label_set_text_fmt(s_lbl_streak, tr(id), n);
}

/* A medianoche cambian el dia de hoy y las frases de "hoy"/"ayer" */
static void streak_days_refresh(void)
{
    bool valid = net_wifi_time_valid();
    int32_t key = -2;
    if (valid) {
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        key = tm.tm_yday;
    }
    if (key != s_week_day_drawn) streak_draw();
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
        const wm_frame_t f = { .w = MAP_W, .h = MAP_H, .top = (int16_t)(MAP_TOP + chip_h + 8),
                               .bottom = MAP_BOTTOM, .margin = 24, .min_ppd = 1.2f, .max_ppd = 9.0f };
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
        /* los km, en el centro de la linea */
        lv_obj_align(s_km, LV_ALIGN_CENTER, ((int32_t)hx + s_inc_x) / 2 - MAP_W / 2,
                     ((int32_t)hy + s_inc_y) / 2 - MAP_H / 2);
    }

    char km[16];
    fmt_km(km, sizeof(km), twin_geo_distance_km(TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, g->lat, g->lon));
    lv_label_set_text_fmt(s_lbl_km, "%s km", km);
    s_clock[0] = '\0';
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
    lv_obj_set_pos(s_chip, 8, MAP_TOP + 8);
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
    lv_obj_set_style_max_width(s_lbl_country, 130, 0);
    lv_label_set_long_mode(s_lbl_country, LV_LABEL_LONG_MODE_DOTS);
    /* el bebe en pequeno: la etiqueta es un boton que vuelve a su pagina */
    lv_obj_t *baby = lv_image_create(row);
    lv_image_set_src(baby, &img_baby_sleep);
    lv_image_set_inner_align(baby, LV_IMAGE_ALIGN_STRETCH); /* 114x185 a 16x26 */
    lv_obj_set_size(baby, 16, 26);
    lv_obj_set_clickable(baby, false);
    lv_obj_set_clickable(s_chip, true);
    lv_obj_set_style_bg_color(s_chip, COL_BG_PHOTO, LV_STATE_PRESSED);
    lv_obj_add_flag(s_chip, UI_FLAG_CLICK_SOUND);
    lv_obj_set_ext_click_area(s_chip, 6);
    lv_obj_add_event_cb(s_chip, chip_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_hosp = lv_label_create(s_chip);
    lv_obj_set_style_text_font(s_lbl_hosp, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_hosp, COL_NAVY_DARK, 0);
    lv_label_set_text(s_lbl_hosp, DEMO_HOSPITAL);

    /* los km, en una pastilla sobre la linea (se centra en country_page_refresh) */
    s_km = overlay(p);
    lv_obj_set_size(s_km, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_km, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_km, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_km, COL_CORAL, 0);
    lv_obj_set_style_border_width(s_km, 2, 0);
    lv_obj_set_style_pad_hor(s_km, 8, 0);
    lv_obj_set_style_pad_ver(s_km, 2, 0);
    s_lbl_km = lv_label_create(s_km);
    lv_obj_set_style_text_font(s_lbl_km, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_km, COL_NAVY_DARK, 0);
    lv_label_set_text(s_lbl_km, "");

    /* pagina 2: logros acumulados (demo) o la racha (modo acompañar) */
    p = s_pg[2] = panel(scr);
    lv_obj_set_user_data(p, (void *)(intptr_t)2);
    s_ach_box = lv_obj_create(p);
    lv_obj_set_size(s_ach_box, 240, PG_H - PG_TOP);
    lv_obj_set_pos(s_ach_box, 0, PG_TOP);
    lv_obj_set_style_bg_opa(s_ach_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ach_box, 0, 0);
    lv_obj_set_style_pad_all(s_ach_box, 0, 0);
    lv_obj_set_scrollable(s_ach_box, false);
    lv_obj_set_clickable(s_ach_box, false);
    s_streak_box = lv_obj_create(p);
    lv_obj_set_size(s_streak_box, 240, PG_H - PG_TOP);
    lv_obj_set_pos(s_streak_box, 0, PG_TOP);
    lv_obj_set_style_bg_opa(s_streak_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_streak_box, 0, 0);
    lv_obj_set_style_pad_all(s_streak_box, 0, 0);
    lv_obj_set_scrollable(s_streak_box, false);
    lv_obj_set_clickable(s_streak_box, false);
    lv_obj_set_hidden(s_streak_box, true);

    /* la racha: titulo, 7 corazones con la inicial del dia y una frase */
    s_lbl_streak_t = text(s_streak_box, &lv_font_es_20, COL_NAVY_DARK, 0, 0);
    lv_obj_align(s_lbl_streak_t, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *c = card(s_streak_box, 36, 76);
    for (int i = 0; i < WEEK_N; i++) {
        int32_t x = 6 + i * 29; /* 7 x 29 = 203 dentro de 216 */
        lv_obj_t *h = s_week[i] = lv_image_create(c);
        lv_image_set_src(h, &img_heart);
        lv_image_set_inner_align(h, LV_IMAGE_ALIGN_STRETCH);
        lv_obj_set_size(h, 26, 23);
        lv_obj_set_pos(h, x + 1, 14);
        lv_obj_set_style_image_recolor(h, COL_OFFLINE, 0);
        lv_obj_set_clickable(h, false);
        s_week_day[i] = text(c, &lv_font_es_12, COL_OFFLINE, 0, 46);
        lv_obj_set_width(s_week_day[i], 28);
        lv_obj_set_x(s_week_day[i], x);
        lv_obj_set_style_text_align(s_week_day[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    s_lbl_streak = text(s_streak_box, &lv_font_es_16, COL_NAVY_DARK, 20, 124);
    lv_obj_set_width(s_lbl_streak, 200);
    lv_obj_set_style_text_align(s_lbl_streak, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_lbl_streak, LV_LABEL_LONG_MODE_WRAP);

    p = s_ach_box;
    s_lbl_ach_t = text(p, &lv_font_es_20, COL_NAVY_DARK, 0, 0);
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, ACH_T_Y);
    s_lbl_ach_sub = text(p, &lv_font_es_12, COL_OFFLINE, 0, 0);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, ACH_SUB_Y);
    for (int i = 0; i < 4; i++) {
        /* hasta 3 lineas de texto (30 + 3 x 16 = 78); antes y 58 / 150, alto 88 */
        c = card(p, 48 + (i / 2) * 82, 78);
        lv_obj_set_size(c, 104, 78);
        lv_obj_set_x(c, 12 + (i % 2) * 112);
        s_tile_num[i] = text(c, &lv_font_es_20, COL_CORAL, 0, 0);
        lv_obj_align(s_tile_num[i], LV_ALIGN_TOP_MID, 0, 6);
        s_tile_cap[i] = text(c, &lv_font_es_12, COL_NAVY_DARK, 0, 0);
        lv_obj_set_width(s_tile_cap[i], 96);
        lv_obj_set_style_text_align(s_tile_cap[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_tile_cap[i], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align(s_tile_cap[i], LV_ALIGN_TOP_MID, 0, 30);
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
    country_page_refresh();
    lv_label_set_text(s_lbl_streak_t, tr(STR_STREAK_TITLE));
    lv_obj_align(s_lbl_streak_t, LV_ALIGN_TOP_MID, 0, 4);
    streak_draw(); /* iniciales y frase en el idioma nuevo */
    lv_label_set_text(s_lbl_ach_t, tr(STR_PG_ACHIEVEMENTS));
    lv_label_set_text(s_lbl_ach_sub, tr(STR_PG_ACH_SUB));
    static const ui_str_t CAP[4] = { STR_PG_MY_DAYS, STR_PG_STREAK, STR_PG_FAMILIES, STR_PG_DAYS };
    for (int i = 0; i < 4; i++) lv_label_set_text(s_tile_cap[i], tr(CAP[i]));
    lv_obj_align(s_lbl_ach_t, LV_ALIGN_TOP_MID, 0, ACH_T_Y);
    lv_obj_align(s_lbl_ach_sub, LV_ALIGN_TOP_MID, 0, ACH_SUB_Y);
}

/* Corazones nuevos del modelo (bit 0 = hoy) */
static void week_refresh(uint8_t mask, uint8_t super, uint8_t rest)
{
    if ((int)mask == s_week_mask && super == s_super_mask && rest == s_rest_mask) return;
    s_week_mask = mask;
    s_super_mask = super;
    s_rest_mask = rest;
    streak_draw();
}

bool ui_pages_status_text(char *buf, size_t n)
{
    if (s_page != 1 || !s_clock[0] || !s_country[0]) return false;
    snprintf(buf, n, tr(STR_PG_TIME_IN), tr_country(s_country), s_clock);
    return true;
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
    bool acc_on = !s_enabled && s->acc != TWIN_ACC_OFF;
    bool acc_changed = acc_on != s_acc_on;
    s_acc_on = acc_on;
    lv_obj_set_hidden(s_ach_box, acc_on);
    lv_obj_set_hidden(s_streak_box, !acc_on);
    if (acc_on) week_refresh(s->acc_week, s->acc_super, s->acc_rest);
    if (acc_changed) streak_draw();
    if (known && (strcmp(cc, s_country) != 0 || demo_changed)) {
        memcpy(s_country, cc, TWIN_COUNTRY_LEN);
        country_page_refresh();
    }
    /* fuera de la demo: bebe + pais (si se sabe) + la racha en modo acompañar */
    int avail = s_enabled ? UI_PAGES_COUNT : !known ? 1 : acc_on ? 3 : 2;
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
