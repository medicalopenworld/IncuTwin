/* Pantalla de la IncuPray (spec incupray). Capa a pantalla completa en lv_layer_top (tapa la barra
 * de navegacion): arriba el titulo, lo que queda y cuantas IncuTwin estan; en medio el mapa con
 * las cabecitas de los bebes que hay ahora en las IncuNest (se pueden tocar); abajo los nombres de
 * las IncuTwin, uno tras otro, y Salir. Tocar un bebe abre su vista: el bebe grande con un anillo
 * que se llena mientras se mantiene el dedo (un corazon por minuto, hasta 10) y +1 al ganar uno.
 * Al salir, o al cerrarse la ventana, da las gracias con los corazones ganados.
 *
 * ui_incupray_poll() (desde el refresco de ui.c, una vez por segundo) saca los avisos de la vispera,
 * de una hora antes y del inicio, el resumen de una IncuPray en la que no se entro, y cambia el
 * boton de Inicio a «IncuPray» mientras esta abierta. Sin horario del servidor no hace nada.
 * Nunca habla de rezar. Todo en la tarea de LVGL. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "incupray.h"
#include "twin_geo.h"
#include "ui_internal.h"
#include "world_map.h"

static const char *TAG = "incupray_ui";

#define SCR_W        240
#define SCR_H        320
#define TOP_H        54
#define MAP_H        200
#define FOOT_Y       (TOP_H + MAP_H)
#define POLL_MS      1000
#define HOLD_TICK_MS 100
#define NAMES_MS     3000
#define HINT_MS      4000
#define RING_D       164
#define COL_LAND     lv_color_hex(0xEEE8D8)
#define COL_SEA      lv_color_hex(0xCFE2F3)

static lv_obj_t *s_root; /* capa; NULL = cerrada */
static lv_obj_t *s_lbl_left, *s_lbl_count, *s_lbl_hint, *s_lbl_name;
static lv_obj_t *s_map, *s_canvas, *s_dot_home;
static uint16_t *s_map_buf;
static lv_obj_t *s_heads[IP_BABIES_MAX];
static lv_obj_t *s_view; /* vista de un bebe */
static lv_obj_t *s_baby_area, *s_arc, *s_baby_img, *s_lbl_country, *s_lbl_hearts;
static lv_timer_t *s_hold_timer;
static ip_live_t s_live; /* lo que se pinta (2 kB: estatico) */
static uint8_t s_name_idx;
static uint32_t s_names_ms, s_hint_ms, s_hold_last_ms, s_poll_ms;
static bool s_hint_temp; /* el aviso de la apuesta tapa "Elige un bebe" unos segundos */
static uint8_t s_hearts_shown;
static bool s_btn_mode; /* el boton de Inicio dice «IncuPray» */

/* Separacion de los bebes del mismo pais (cabecitas de 26 x 24) */
static const int8_t SPREAD[8][2] = { { 0, 0 },  { 16, 0 },  { -16, 0 }, { 0, -15 },
                                     { 0, 15 }, { 14, 13 }, { -14, -13 }, { 14, -13 } };

static const lv_image_dsc_t *head_img(uint8_t sex)
{
    return sex == TWIN_SEX_FEMALE ? &img_baby_head_girl : &img_baby_head_boy;
}

/* ---------------------------------------------------------------- mapa */

static void hint_refresh(void)
{
    if (s_hint_temp && lv_tick_elaps(s_hint_ms) < HINT_MS) return;
    s_hint_temp = false;
    lv_label_set_text(s_lbl_hint, tr(s_live.n_babies ? STR_IP_PICK : STR_IP_NO_BABIES));
}

static void map_render(void)
{
    float lat_min = TWIN_GEO_HOME_LAT, lat_max = TWIN_GEO_HOME_LAT;
    float lon_min = TWIN_GEO_HOME_LON, lon_max = TWIN_GEO_HOME_LON;
    for (uint8_t i = 0; i < s_live.n_babies; i++) {
        const twin_geo_t *g = twin_geo_find(s_live.babies[i].country);
        if (!g) continue;
        if (g->lat < lat_min) lat_min = g->lat;
        if (g->lat > lat_max) lat_max = g->lat;
        if (g->lon < lon_min) lon_min = g->lon;
        if (g->lon > lon_max) lon_max = g->lon;
    }
    const wm_frame_t f = { .w = SCR_W, .h = MAP_H, .top = 26, .bottom = 6, .margin = 22,
                           .min_ppd = 1.2f, .max_ppd = 6.0f };
    wm_view_t v;
    wm_fit(&v, &f, lat_max, lon_min, lat_min, lon_max);
    if (s_map_buf) {
        wm_render(s_map_buf, SCR_W, MAP_H, &v, lv_color_to_u16(COL_LAND), lv_color_to_u16(COL_SEA));
        lv_obj_invalidate(s_canvas);
    }
    float x, y;
    wm_project(&v, TWIN_GEO_HOME_LAT, TWIN_GEO_HOME_LON, &x, &y);
    lv_obj_set_pos(s_dot_home, (int32_t)x - 4, (int32_t)y - 4);

    for (uint8_t i = 0; i < IP_BABIES_MAX; i++) {
        const twin_geo_t *g = i < s_live.n_babies ? twin_geo_find(s_live.babies[i].country) : NULL;
        lv_obj_set_hidden(s_heads[i], g == NULL);
        if (!g) continue;
        uint8_t k = 0; /* bebes anteriores del mismo pais */
        for (uint8_t j = 0; j < i; j++) {
            if (!strncmp(s_live.babies[j].country, s_live.babies[i].country, 2)) k++;
        }
        int32_t r = 1 + k / 8; /* mas de 8 en un pais: otro anillo */
        wm_project(&v, g->lat, g->lon, &x, &y);
        lv_image_set_src(s_heads[i], head_img(s_live.babies[i].sex));
        lv_obj_set_pos(s_heads[i], (int32_t)x - 13 + SPREAD[k % 8][0] * r, (int32_t)y - 12 + SPREAD[k % 8][1] * r);
    }
    hint_refresh();
}

static void names_next(void)
{
    if (!s_live.n_names) {
        lv_label_set_text(s_lbl_name, "");
        return;
    }
    s_name_idx = (uint8_t)((s_name_idx + 1) % s_live.n_names);
    lv_label_set_text_fmt(s_lbl_name, "• %s •", s_live.names[s_name_idx]);
    s_names_ms = lv_tick_get();
}

/* Lo ultimo de incupray/live: repinta el mapa solo si han cambiado los bebes */
static void live_refresh(bool force)
{
    static ip_live_t l;
    incupray_live(&l);
    bool babies = force || l.n_babies != s_live.n_babies ||
                  memcmp(l.babies, s_live.babies, sizeof(ip_baby_t) * l.n_babies) != 0;
    bool names = force || l.n_names != s_live.n_names;
    s_live = l;
    lv_label_set_text_fmt(s_lbl_count, tr(STR_IP_COUNT), (unsigned)s_live.count);
    if (babies) map_render();
    if (names) {
        s_name_idx = (uint8_t)(s_live.n_names ? s_live.n_names - 1 : 0);
        names_next();
    } else if (lv_tick_elaps(s_names_ms) >= NAMES_MS) {
        names_next();
    }
}

/* ---------------------------------------------------------- vista de un bebe */

static void plus_y_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)(v > 10 ? LV_OPA_COVER : v * 25), 0);
}

/* +1 que sube desde el bebe y se desvanece */
static void plus_one(void)
{
    lv_obj_t *p = lv_obj_create(s_view);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(p, 3, 0);
    lv_obj_set_clickable(p, false);
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, "+1");
    lv_obj_set_style_text_font(l, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(l, COL_CORAL, 0);
    lv_obj_t *h = lv_image_create(p);
    lv_image_set_src(h, &img_heart);
    lv_obj_align(p, LV_ALIGN_TOP_MID, 40, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, p);
    lv_anim_set_exec_cb(&a, plus_y_cb);
    lv_anim_set_values(&a, 60, 0);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&a);
}

static void hearts_label(uint8_t hearts)
{
    lv_label_set_text_fmt(s_lbl_hearts, tr(STR_IP_HEARTS), (unsigned)hearts, (unsigned)IP_HEARTS_MAX);
}

static void hold_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t now = lv_tick_get();
    uint32_t dt = now - s_hold_last_ms;
    s_hold_last_ms = now;
    if (lv_obj_has_state(s_baby_area, LV_STATE_PRESSED) && dt < 1000) incupray_hold(dt);
    uint8_t hearts = incupray_hearts();
    uint32_t held = incupray_held_s();
    lv_arc_set_value(s_arc, hearts >= IP_HEARTS_MAX ? IP_SECONDS_PER_HEART : (int32_t)(held % IP_SECONDS_PER_HEART));
    if (hearts > s_hearts_shown) {
        s_hearts_shown = hearts;
        plus_one();
        ESP_LOGI(TAG, "+1 corazon (%u de %u)", hearts, IP_HEARTS_MAX);
    }
    hearts_label(hearts);
}

static void view_show(bool on)
{
    lv_obj_set_hidden(s_view, !on);
    if (on) {
        s_hold_last_ms = lv_tick_get();
        lv_timer_resume(s_hold_timer);
    } else {
        lv_timer_pause(s_hold_timer);
    }
}

static void head_clicked(lv_event_t *e)
{
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (i >= s_live.n_babies) return;
    const ip_baby_t *b = &s_live.babies[i];
    lv_image_set_src(s_baby_img, b->sex == TWIN_SEX_FEMALE ? &img_baby_girl_sleep : &img_baby_sleep);
    lv_label_set_text(s_lbl_country, tr_country(b->country));
    hearts_label(incupray_hearts());
    ESP_LOGI(TAG, "con un bebe de %s", b->country);
    view_show(true);
}

static void map_clicked(lv_event_t *e)
{
    (void)e;
    view_show(false);
}

/* ---------------------------------------------------------- cerrar */

static void thanks_ok(lv_event_t *e)
{
    lv_msgbox_close(lv_event_get_user_data(e));
}

static void root_deleted(lv_event_t *e)
{
    (void)e;
    if (s_hold_timer) {
        lv_timer_delete(s_hold_timer);
        s_hold_timer = NULL;
    }
    heap_caps_free(s_map_buf);
    s_map_buf = NULL;
}

static void finish(void)
{
    if (!s_root) return;
    uint8_t hearts = incupray_hearts();
    uint16_t count = s_live.count;
    incupray_leave();
    lv_timer_pause(s_hold_timer);
    lv_obj_set_hidden(s_root, true);
    lv_obj_delete_async(s_root); /* puede venir del boton Salir, hijo de la capa */
    s_root = NULL;
    ESP_LOGI(TAG, "fuera: %u corazones", hearts);

    char txt[96];
    int n = snprintf(txt, sizeof(txt), tr(STR_IP_THANKS_D), (unsigned)hearts);
    if (count && n > 0 && (size_t)n < sizeof(txt) - 1) {
        txt[n++] = '\n';
        snprintf(txt + n, sizeof(txt) - (size_t)n, tr(STR_IP_THANKS_ALL), (unsigned)count);
    }
    ui_msgbox(tr(STR_IP_THANKS), txt, tr(STR_OK), COL_CORAL, thanks_ok, NULL);
}

static void exit_clicked(lv_event_t *e)
{
    (void)e;
    finish();
}

/* ---------------------------------------------------------- construir */

static lv_obj_t *button(lv_obj_t *parent, const char *txt, lv_color_t bg, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 104, 32);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollable(o, false);
    return o;
}

static void build_view(void)
{
    s_view = plain(s_root);
    lv_obj_set_size(s_view, SCR_W, SCR_H - TOP_H);
    lv_obj_set_pos(s_view, 0, TOP_H);
    lv_obj_set_style_bg_color(s_view, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_view, LV_OPA_COVER, 0);
    lv_obj_set_clickable(s_view, true);

    s_lbl_country = lv_label_create(s_view);
    lv_obj_set_style_text_font(s_lbl_country, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(s_lbl_country, COL_NAVY, 0);
    lv_obj_align(s_lbl_country, LV_ALIGN_TOP_MID, 0, 0);

    /* zona del dedo: el anillo y el bebe no son pulsables, la pulsacion es de la zona */
    s_baby_area = plain(s_view);
    lv_obj_set_size(s_baby_area, RING_D + 16, RING_D + 4);
    lv_obj_align(s_baby_area, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_clickable(s_baby_area, true);
    lv_obj_add_flag(s_baby_area, LV_OBJ_FLAG_PRESS_LOCK); /* sigue pulsado aunque el dedo se mueva un poco */
    s_arc = lv_arc_create(s_baby_area);
    lv_obj_set_size(s_arc, RING_D, RING_D);
    lv_obj_center(s_arc);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_range(s_arc, 0, IP_SECONDS_PER_HEART);
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, COL_HALO_CALL, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_clickable(s_arc, false);
    s_baby_img = lv_image_create(s_baby_area);
    lv_image_set_src(s_baby_img, &img_baby_sleep);
    lv_obj_center(s_baby_img);
    lv_obj_set_clickable(s_baby_img, false);

    lv_obj_t *hold = lv_label_create(s_view);
    lv_label_set_text(hold, tr(STR_IP_HOLD));
    lv_obj_set_style_text_font(hold, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(hold, COL_NAVY_DARK, 0);
    lv_obj_align(hold, LV_ALIGN_TOP_MID, 0, 194);

    lv_obj_t *row = plain(s_view);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 5, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 212);
    lv_obj_t *hi = lv_image_create(row);
    lv_image_set_src(hi, &img_heart);
    s_lbl_hearts = lv_label_create(row);
    lv_obj_set_style_text_font(s_lbl_hearts, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_hearts, COL_CORAL, 0);

    lv_obj_t *back = button(s_view, tr(STR_IP_MAP), COL_NAVY, map_clicked);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 12, -2);
    lv_obj_t *ex = button(s_view, tr(STR_IP_EXIT), COL_OFFLINE, exit_clicked);
    lv_obj_align(ex, LV_ALIGN_BOTTOM_RIGHT, -12, -2);
    lv_obj_set_hidden(s_view, true);
}

static void build(void)
{
    s_root = plain(lv_layer_top());
    lv_obj_set_size(s_root, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_root, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_clickable(s_root, true); /* que no pase nada a la pantalla de debajo */
    lv_obj_add_event_cb(s_root, root_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "IncuPray");
    lv_obj_set_style_text_font(title, &lv_font_es_20, 0);
    lv_obj_set_style_text_color(title, COL_NAVY, 0);
    lv_obj_set_pos(title, 10, 6);
    s_lbl_left = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_lbl_left, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_left, COL_CORAL, 0);
    lv_obj_align(s_lbl_left, LV_ALIGN_TOP_RIGHT, -10, 10);
    s_lbl_count = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_lbl_count, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_count, COL_NAVY_DARK, 0);
    lv_obj_set_pos(s_lbl_count, 10, 33);

    s_map = plain(s_root);
    lv_obj_set_size(s_map, SCR_W, MAP_H);
    lv_obj_set_pos(s_map, 0, TOP_H);
    lv_obj_set_style_bg_color(s_map, COL_SEA, 0);
    lv_obj_set_style_bg_opa(s_map, LV_OPA_COVER, 0);
    s_canvas = lv_canvas_create(s_map);
    s_map_buf = heap_caps_malloc((size_t)SCR_W * MAP_H * 2, MALLOC_CAP_SPIRAM);
    if (s_map_buf) {
        memset(s_map_buf, 0, (size_t)SCR_W * MAP_H * 2);
        lv_canvas_set_buffer(s_canvas, s_map_buf, SCR_W, MAP_H, LV_COLOR_FORMAT_RGB565);
    } else {
        ESP_LOGW(TAG, "sin memoria para el mapa: solo el mar");
    }
    s_dot_home = plain(s_map);
    lv_obj_set_size(s_dot_home, 8, 8);
    lv_obj_set_style_radius(s_dot_home, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot_home, COL_NAVY, 0);
    lv_obj_set_style_bg_opa(s_dot_home, LV_OPA_COVER, 0);
    for (uint8_t i = 0; i < IP_BABIES_MAX; i++) {
        s_heads[i] = lv_image_create(s_map);
        lv_image_set_src(s_heads[i], &img_baby_head_boy);
        lv_obj_set_clickable(s_heads[i], true);
        lv_obj_set_ext_click_area(s_heads[i], 6);
        lv_obj_add_event_cb(s_heads[i], head_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_hidden(s_heads[i], true);
    }
    s_lbl_hint = lv_label_create(s_map);
    lv_obj_set_style_text_font(s_lbl_hint, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_lbl_hint, COL_NAVY_DARK, 0);
    lv_obj_set_style_bg_color(s_lbl_hint, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_lbl_hint, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_lbl_hint, 8, 0);
    lv_obj_set_style_pad_hor(s_lbl_hint, 6, 0);
    lv_obj_set_style_pad_ver(s_lbl_hint, 2, 0);
    lv_obj_align(s_lbl_hint, LV_ALIGN_TOP_MID, 0, 4);

    s_lbl_name = lv_label_create(s_root);
    lv_obj_set_size(s_lbl_name, 220, 18);
    lv_label_set_long_mode(s_lbl_name, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_lbl_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_lbl_name, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_name, COL_NAVY, 0);
    lv_obj_set_pos(s_lbl_name, 10, FOOT_Y + 6);
    lv_obj_t *ex = button(s_root, tr(STR_IP_EXIT), COL_OFFLINE, exit_clicked);
    lv_obj_align(ex, LV_ALIGN_BOTTOM_MID, 0, -6);

    build_view();
    s_hold_timer = lv_timer_create(hold_cb, HOLD_TICK_MS, NULL);
    lv_timer_pause(s_hold_timer);
}

/* ---------------------------------------------------------- API */

bool ui_incupray_active(void) { return s_root != NULL; }

void ui_incupray_open(void)
{
    if (s_root) return;
    ip_window_t w;
    if (!incupray_window(&w) || ip_phase(&w, incupray_now()) != IP_OPEN) return;
    build();
    uint16_t paid = 0;
    bool first = incupray_join(&paid);
    s_hearts_shown = incupray_hearts();
    memset(&s_live, 0, sizeof(s_live));
    live_refresh(true);
    if (first) { /* la apuesta, o que hoy es gratis, unos segundos en lugar de "Elige un bebe" */
        if (paid) lv_label_set_text_fmt(s_lbl_hint, tr(STR_IP_STAKE), (unsigned)paid);
        else lv_label_set_text(s_lbl_hint, tr(STR_IP_FREE));
        s_hint_temp = true;
        s_hint_ms = lv_tick_get();
    }
    uint32_t left = ip_remaining_s(&w, incupray_now());
    lv_label_set_text_fmt(s_lbl_left, tr(STR_IP_LEFT), (unsigned)(left / 60), (unsigned)(left % 60));
    ESP_LOGI(TAG, "dentro (%s, apuesta %u)", first ? "primera vez" : "otra vez", paid);
}

static void push_notice(ip_notice_t n, const ip_window_t *w)
{
    char txt[64];
    switch (n) {
    case IP_NOTICE_EVE: {
        char hm[8] = "";
        time_t t = (time_t)w->start;
        struct tm tm;
        if (localtime_r(&t, &tm)) strftime(hm, sizeof(hm), "%H:%M", &tm);
        snprintf(txt, sizeof(txt), tr(STR_IP_EVE), hm);
        ui_notice_push_text(txt, false);
        break;
    }
    case IP_NOTICE_HOUR: ui_notice_push_text(tr(STR_IP_HOUR), false); break;
    case IP_NOTICE_START:
        if (!s_root) ui_notice_push_text(tr(STR_IP_START), true);
        break;
    default: break;
    }
}

void ui_incupray_poll(void)
{
    if (lv_tick_elaps(s_poll_ms) < POLL_MS) return;
    s_poll_ms = lv_tick_get();

    ip_window_t w;
    bool has = incupray_window(&w);
    uint32_t now = incupray_now();
    bool open = has && ip_phase(&w, now) == IP_OPEN;
    if (open != s_btn_mode) {
        s_btn_mode = open;
        ui_home_incupray_set(open);
    }
    ip_notice_t n = incupray_take_notice();
    if (n != IP_NOTICE_NONE) push_notice(n, &w);
    ip_summary_t sm;
    if (!s_root && incupray_take_missed_summary(&sm)) {
        char txt[64];
        snprintf(txt, sizeof(txt), tr(STR_IP_SUMMARY), (unsigned)sm.count, (unsigned long)sm.hearts);
        ui_notice_push_text(txt, false);
    }

    if (!s_root) return;
    if (!open) { /* se ha cerrado la ventana (o la demo) con la pantalla abierta */
        finish();
        return;
    }
    uint32_t left = ip_remaining_s(&w, now);
    lv_label_set_text_fmt(s_lbl_left, tr(STR_IP_LEFT), (unsigned)(left / 60), (unsigned)(left % 60));
    live_refresh(false);
    if (s_hint_temp) hint_refresh();
}
