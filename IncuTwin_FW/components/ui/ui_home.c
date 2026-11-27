/* Pantalla principal (spec twin-display, hmi-navegacion-avisos). Layout vertical 240x320:
 *   barra superior : marca a media escala a la izquierda | boton del pais a la derecha (lleva al
 *                    mapa y, desde el mapa, vuelve al bebe). El WiFi se cambia en Ajustes.
 *   centro         : bebe + halo dentro de la pantallita (6..232 x 36..228, estilo de la app) |
 *                    incubadora vacia | con sus papas (dos tonos de piel), estas a pantalla completa
 *   cuerpo del bebe: nombre
 *   fila (234..270): simbolos del tratamiento en curso (solo los activos, juntos a la
 *                    izquierda) + boton "Coge mi mano" (o el texto del gesto). Si hoy aun no le
 *                    has dado la mano, el boton late con un anillo coral suave.
 *   273..279       : puntos de pagina (ui_pages.c)
 *   280..320       : barra de navegacion (ui_nav.c); nada de Inicio por debajo de UI_NAV_Y
 * Sin barra de estado: lo que pasa se ve en el dibujo, en el texto del gesto y en el Historial.
 * Al completar el primer gesto del dia, un corazon vuela a la racha (ui_streak_fx.c).
 * En demo se desliza a izquierda/derecha entre tres paginas (ui_pages.c).
 * El latido solo suena al tocar el icono del corazon (ritmo real del bebe).
 * El gesto "Coge mi mano" (spec hold-my-hand) lo lleva hand_hold; aqui solo se pinta. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "acc_logic.h"
#include "affection.h"
#include "assets.h"
#include "baby_name.h"
#include "baby_widget.h"
#include "esp_log.h"
#include "hand_hold.h"
#include "name_logic.h"
#include "net_wifi.h"
#include "rewards.h"
#include "sdkconfig.h"
#include "sound.h"
#include "telemetry.h"
#include "twin_logic.h"
#include "ui_internal.h"
#include "ui_name.h"
#include "ui_nowifi.h"
#include "assets_nowifi.h"
#include "ui_pages.h"
#include "ui_toy.h"
#include "usage_stats.h"


/* Estilo «IncuTwin de bolsillo» (como la app): el bebe, su halo, el fondo comprado y el gesto van
 * dentro de una «pantallita» con marco tinta y sombra dura, mas rectangular que la de la app
 * (esquinas de 8, Pablo 27-nov-2026). La pantallita son dos objetos: el fondo (debajo del halo) y el
 * marco (encima del bebe y del gesto), para no cambiar de padre ni de coordenadas lo de dentro. El
 * bebe baja 10 px para caber entero (con la respiracion, y 37..225) dentro del marco (36..228). */
#define STAGE_X 6
#define STAGE_Y 36
#define STAGE_W 226
#define STAGE_H 193
#define STAGE_R 8
#define BABY_X 63   /* sprite de 114x185 */
#define BABY_Y 40
#define BABY_CX (BABY_X + 57)
#define BABY_CY (BABY_Y + 92)
#define ICON_D 36   /* antes 38; la fila va bajo la pantallita (y su sombra) */
#define ICON_Y 234
#define HAND_X 136  /* «Coge mi mano» ocupa el resto de la fila, como en la app */
#define HAND_W 94
/* imagenes "con sus papas": su dibujo (medido) acaba en y 283 / 301; se suben para que acabe
 * en la barra de navegacion sin cortar el corazon de arriba */
#define PARENTS_Y       (-3)
#define PARENTS_BLACK_Y (-18)

/* gesto "Coge mi mano" */
#define RING_D        186 /* cabe en la pantallita */
#define NAME_Y        198
#define COUNTRY_LBL_W 64  /* nombre del pais en la barra de arriba: que no tape el gorro */
#define N_HEARTS      4
#define HH_TICK_MS    100
#define COL_RING_BG   lv_color_hex(0xF5C4B3)
#define COL_RING_DIM  lv_color_hex(0xEBA8A0)
#define COL_BG_THANKS COL_CORAL_SOFT
#define HOLD_H        ICON_D /* texto del gesto: en el sitio de la fila */
/* Vueltas del anillo: cada minuto extra pinta otra vuelta encima, en el color siguiente.
 * Colores muy distintos entre vueltas seguidas para que se vea el cambio:
 * coral -> azul de la marca -> ambar -> verde -> coral... */
static const uint32_t LAP_COLS[] = { 0xDE6F63, 0x054E92, 0xF5A93B, 0x3F9E63 };
#define N_LAP_COLS (sizeof(LAP_COLS) / sizeof(LAP_COLS[0]))

typedef enum { HALO_OFF = 0, HALO_CALM, HALO_WARM, HALO_PHOTO, HALO_CALL } halo_mode_t;
typedef enum { VIEW_EMPTY = 0, VIEW_BABY, VIEW_PARENTS, VIEW_NOWIFI } home_view_t;

/* Bebe sin WiFi (ui_nowifi.c): sale tras este rato seguido sin WiFi, para que el arranque y
 * los cortes de unos segundos no lo enseñen; al volver el WiFi celebra y vuelve a lo de siempre */
#define NOWIFI_DELAY_MS 8000
#define NOWIFI_TICK_MS  1000
static uint32_t s_nowifi_since; /* tick en que se vio sin WiFi (0 = hay WiFi) */
static int8_t s_nowifi_force;   /* consola `sinwifi`: 0 = de verdad, 1 = nino, 2 = nina */
static ui_nowifi_t *s_nw;

static lv_obj_t *s_scr;
static lv_obj_t *s_stage_bg, *s_stage_frame; /* la pantallita: fondo y marco */
static lv_color_t s_bg_col;                  /* fondo de lo del bebe (pantallita o pantalla) */
static home_view_t s_view = VIEW_EMPTY;
static lv_obj_t *s_halo, *s_empty_img, *s_parents_img, *s_demo_badge;
static lv_obj_t *s_info;          /* burbuja con la explicacion del simbolo tocado */
static lv_timer_t *s_info_timer;
static lv_obj_t *s_icon_thermo, *s_icon_photo, *s_icon_heart;
static lv_obj_t *s_btn_hand, *s_lbl_hand;
static lv_obj_t *s_btn_back; /* "Volver al bebé" en el agradecimiento */
static bool s_nudge;         /* el boton "Coge mi mano" late: hoy aun no le has dado la mano */
static lv_obj_t *s_garland;  /* guirnalda de Navidad en lo alto (personalizar) */
static halo_mode_t s_halo_mode = HALO_CALM;
static bool s_awake_real; /* para restaurar al terminar el gesto */

static hand_hold_t s_hh;
static lv_obj_t *s_ring, *s_target, *s_lbl_hold, *s_name, *s_hearts[N_HEARTS];
static lv_obj_t *s_thanks_img, *s_country, *s_country_flag, *s_country_lbl;
static int32_t s_ring_lap = -1; /* vuelta pintada; -1 = forzar colores */
static bool s_super_sent; /* super corazon ya avisado en este gesto */
static lv_timer_t *s_hb_timer; /* latido sonando tras tocar el icono del corazon */

#define HB_TAP_MS 10000
#define INFO_MS   6000 /* lo que dura la burbuja de informacion */
#define COL_HEART_ON lv_color_hex(0xFBE3DF)
static int s_hh_shown = -1;     /* estado pintado; -1 = forzar repintado */
static twin_snapshot_t s_last;  /* ultimo estado (gesto, burbujas, racha) */
static bool s_have_last;
static bool s_row_shown; /* la fila se ve: bebe a la vista y sin gesto */
static uint32_t s_farewell_ms; /* lv_tick al entrar en la despedida del modo acompañar */
static bool s_farewell;
#define FAREWELL_TOUCH_MIN_MS 5000 /* que no la cierre un toque fantasma nada mas salir */

static void hold_refresh(bool force);

/* Telemetria `gesture`: UN evento por mano puesta, al terminar (el gesto y el super corazon son el
 * mismo dedo). Solo si llego a contar como gesto fuera de la demo y con Acompañar encendido;
 * sec = dedo acumulado real, guardado en cada tick para no perderlo al volver a reposo. */
static bool s_tel_gesture;
static uint32_t s_tel_ms;
static char s_tel_inc[TWIN_INCUBATOR_ID_LEN];

static void row_apply(bool shown);
static void scr_bg_restore(void);
static void overlay_refresh(void);
static void nav_apply(void);
static void name_clicked_cb(lv_event_t *e);
static void name_done(void);
static void name_maybe_ask(void);

/* ------------------------------------------------------------ pantallita */

/* Se ve con el bebe a la vista, salvo en el agradecimiento (su dibujo es a pantalla completa) */
static bool stage_visible(void)
{
    return s_view == VIEW_BABY && hh_state(&s_hh) != HH_THANKS;
}

/* Fondo de lo del bebe (el comprado, la fototerapia, el del gesto): dentro de la pantallita si se
 * ve; si no (IncuNest vacia, con sus papas, agradecimiento, sin WiFi), en toda la pantalla, como
 * antes. Fuera de la pantallita, el rosa lavanda de la app. */
static void home_bg_apply(void)
{
    if (!s_stage_bg || !s_stage_frame) return;
    bool st = stage_visible();
    if (lv_obj_is_hidden(s_stage_bg) == st) {
        lv_obj_set_hidden(s_stage_bg, !st);
        lv_obj_set_hidden(s_stage_frame, !st);
    }
    lv_obj_set_style_bg_color(s_stage_bg, s_bg_col, 0);
    lv_obj_set_style_bg_color(s_scr, st ? COL_BG : s_bg_col, 0);
}

static void home_bg(lv_color_t c)
{
    s_bg_col = c;
    home_bg_apply();
}

static lv_obj_t *stage_part(bool frame)
{
    lv_obj_t *o = lv_obj_create(s_scr);
    lv_obj_set_size(o, STAGE_W, STAGE_H);
    lv_obj_set_pos(o, STAGE_X, STAGE_Y);
    lv_obj_set_style_radius(o, STAGE_R, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false); /* los toques llegan al bebe */
    if (frame) {
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(o, COL_INK, 0);
        lv_obj_set_style_border_width(o, TOY_BORDER, 0);
    } else {
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        toy_shadow(o, TOY_SHADOW_M);
    }
    lv_obj_set_hidden(o, true);
    return o;
}

/* ------------------------------------------------------------------ halo */

static void halo_opa_cb(void *var, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

static void halo_start_anim(uint32_t period)
{
    lv_anim_delete(s_halo, halo_opa_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_halo);
    lv_anim_set_exec_cb(&a, halo_opa_cb);
    lv_anim_set_values(&a, LV_OPA_20, LV_OPA_60);
    lv_anim_set_duration(&a, period);
    lv_anim_set_reverse_duration(&a, period);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void halo_set_mode(halo_mode_t m)
{
    if (m == s_halo_mode) return;
    s_halo_mode = m;
    lv_color_t col = ui_theme_halo_calm();
    uint32_t period = 2600;
    switch (m) {
    case HALO_OFF:
        lv_obj_set_hidden(s_halo, true);
        home_bg(ui_theme_bg());
        return;
    case HALO_WARM:  col = COL_HALO_WARM; break;
    case HALO_PHOTO: col = COL_HALO_PHOTO; period = 2000; break;
    case HALO_CALL:  col = COL_HALO_CALL; period = 1200; break;
    default: break;
    }
    lv_obj_set_hidden(s_halo, false);
    lv_obj_set_style_bg_color(s_halo, col, 0);
    home_bg(m == HALO_PHOTO ? COL_BG_PHOTO : ui_theme_bg());
    halo_start_anim(period);
}

/* ----------------------------------------------------------- state icons */

static lv_obj_t *state_icon_create(lv_obj_t *parent, int32_t x, const void *src)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, ICON_D, ICON_D);
    lv_obj_set_pos(c, x, ICON_Y);
    toy_box(c, LV_RADIUS_CIRCLE, TOY_SHADOW_S); /* circulo con borde tinta y sombra dura, como la app */
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_t *icon = src ? lv_image_create(c) : toy_heart_img(c, TOY_HEART_FULL, 3); /* NULL: latido */
    if (src) lv_image_set_src(icon, src);
    lv_obj_center(icon);
    return c;
}

/* Tratamiento: activo = circulo relleno de su color con el icono en blanco; inactivo = atenuado */
static void treat_icon_set(lv_obj_t *c, bool on, lv_color_t col)
{
    lv_obj_t *icon = lv_obj_get_child(c, 0);
    lv_obj_set_style_bg_color(c, on ? col : COL_CARD, 0);
    lv_obj_set_style_image_recolor(icon, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_opa(icon, on ? LV_OPA_COVER : LV_OPA_50, 0);
}

/* ----------------------------------------------------------- info */

static void info_hide(void)
{
    if (s_info_timer) {
        lv_timer_delete(s_info_timer);
        s_info_timer = NULL;
    }
    lv_obj_set_hidden(s_info, true);
}

static void info_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_info_timer = NULL; /* el timer de una vez se borra solo */
    lv_obj_set_hidden(s_info, true);
}

/* Burbuja encima de la fila de simbolos, con el borde del color del simbolo; se va sola */
static void info_show(const char *txt, lv_color_t col)
{
    if (s_info_timer) lv_timer_delete(s_info_timer);
    lv_label_set_text(s_info, txt);
    lv_obj_set_style_border_color(s_info, col, 0);
    lv_obj_set_hidden(s_info, false);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_update_layout(s_info);
    lv_obj_set_y(s_info, ICON_Y - 6 - lv_obj_get_height(s_info));
    s_info_timer = lv_timer_create(info_timer_cb, INFO_MS, NULL);
    lv_timer_set_repeat_count(s_info_timer, 1);
}

static void thermo_icon_clicked(lv_event_t *e)
{
    (void)e;
    if (!s_have_last) return;
    switch (s_last.inc.thermo) {
    case TWIN_THERMO_HEATING: info_show(tr(STR_INFO_HEATING), COL_WARM); break;
    default:                  info_show(tr(STR_INFO_STABLE), COL_OK); break;
    }
}

static void photo_icon_clicked(lv_event_t *e)
{
    (void)e;
    info_show(tr(STR_INFO_PHOTO), COL_PHOTO);
}

/* ------------------------------------------------------------- handlers */

/* Caricia y cosquillitas (niveles de afecto): el dedo sobre el bebe, sin el gesto en marcha */
static aff_stroke_t s_aff;
static bool s_aff_on;          /* hay un toque en curso que mide aff_* */
static aff_kind_t s_aff_best;  /* lo ya contado y pintado en este toque */
static bool s_swipe_guard;     /* el toque en curso empezo sobre el bebe y el nivel abre la caricia */

static rw_level_t aff_level(void) { return rw_level(ui_rewards()->earned); }

/* Primera vez que el toque llega a caricia o a cosquillas: el bebe reacciona y se cuenta (en la demo
 * no, como el resto del uso) */
static void aff_reached(aff_kind_t k)
{
    if (k <= s_aff_best) return;
    s_aff_best = k;
    info_hide();
    baby_react(k == AFF_TICKLES ? BABY_REACT_TICKLES : BABY_REACT_CARESS);
    if (!(s_have_last && s_last.demo)) usage_affection(k);
}

/* "Coge mi mano": el dedo sobre el bebe sostiene el gesto y cuenta uso; con el gesto en reposo, acaricia */
static void baby_touched(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    bool idle = hh_state(&s_hh) == HH_IDLE;
    lv_point_t p = { 0, 0 };
    if (lv_indev_active()) lv_indev_get_point(lv_indev_active(), &p);
    if (c == LV_EVENT_PRESSED) s_swipe_guard = aff_open(aff_level(), AFF_CARESS);
    if (c == LV_EVENT_PRESSED && idle) {
        s_aff_on = true;
        s_aff_best = AFF_NONE;
        aff_down(&s_aff, (int16_t)p.x, (int16_t)p.y, lv_tick_get());
        info_show(tr(STR_INFO_PRESS_HAND), COL_CORAL); /* el gesto empieza con el boton */
        return;
    }
    if (c == LV_EVENT_PRESSING) {
        if (s_aff_on && idle) aff_reached(aff_move(&s_aff, (int16_t)p.x, (int16_t)p.y, lv_tick_get(), aff_level()));
        return;
    }
    if (c == LV_EVENT_PRESSED) {
        info_hide();
        usage_hand_set(true);
        hh_touch(&s_hh, true, lv_tick_get());
        hold_refresh(false);
    } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        if (s_aff_on) {
            aff_up(&s_aff);
            s_aff_on = false;
        }
        if (c == LV_EVENT_RELEASED) s_swipe_guard = false; /* tras PRESS_LOST lo limpia el tick */
        usage_hand_set(false);
        hh_touch(&s_hh, false, lv_tick_get());
        hold_refresh(false);
    }
}

/* Latido que se ve: el corazon del icono crece un 25 % y vuelve en cada pulsacion (v 0..1000 es un
 * latido entero: sube rapido, baja y descansa el resto) */
static void beat_cb(void *var, int32_t v)
{
    int32_t up = v < 150 ? v * 1000 / 150 : v < 450 ? (450 - v) * 1000 / 300 : 0;
    lv_image_set_scale((lv_obj_t *)var, (uint32_t)(256 + 64 * up / 1000));
}

static void beat_set(bool on, uint16_t bpm)
{
    lv_obj_t *icon = lv_obj_get_child(s_icon_heart, 0);
    lv_anim_delete(icon, beat_cb);
    lv_image_set_scale(icon, LV_SCALE_NONE);
    if (!on || bpm == 0) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, icon);
    lv_anim_set_exec_cb(&a, beat_cb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 60000u / bpm); /* el ritmo real del bebe */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void heart_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_hb_timer = NULL; /* el timer de una vez se borra solo */
    sound_heartbeat(false);
    beat_set(false, 0);
    lv_obj_set_style_bg_color(s_icon_heart, COL_CARD, 0);
}

/* Icono del corazon: suena y se ve el latido al ritmo real del bebe 10 s; otro toque lo para */
static void heart_sound_set(bool on)
{
    if (s_hb_timer) {
        lv_timer_delete(s_hb_timer);
        s_hb_timer = NULL;
    }
    sound_heartbeat(on);
    beat_set(on, s_have_last ? s_last.inc.bpm : 0);
    lv_obj_set_style_bg_color(s_icon_heart, on ? COL_HEART_ON : COL_CARD, 0);
    if (on) {
        s_hb_timer = lv_timer_create(heart_timer_cb, HB_TAP_MS, NULL);
        lv_timer_set_repeat_count(s_hb_timer, 1);
    }
}

static void heart_icon_clicked(lv_event_t *e)
{
    (void)e;
    bool pulse = s_have_last && s_last.show_baby && s_last.inc.bpm > 0;
    if (pulse) {
        /* sin el numero: el latido se oye y se ve al ritmo real */
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "%s", tr(STR_INFO_HEART));
        if (s_last.inc.bpm > TWIN_BPM_FAST && n > 0 && n < (int)sizeof(buf)) {
            snprintf(buf + n, sizeof(buf) - n, ". %s", tr(STR_INFO_HEART_FAST));
        }
        info_show(buf, COL_CORAL);
    }
    heart_sound_set(pulse && !s_hb_timer);
}

/* Deslizar entre paginas (solo en demo). No a mitad de un gesto ya completado. */
/* Tras cambiar de pagina: fuera burbuja y latido, y repintar (el bebe se va o vuelve) */
static void page_changed(void)
{
    info_hide();
    if (s_hb_timer) heart_sound_set(false);
    if (s_have_last) {
        twin_snapshot_t s = s_last;
        ui_home_apply(&s); /* el bebe deja de estar (o vuelve a estar) a la vista */
    }
}

static void scr_gesture(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    hh_state_t st = hh_state(&s_hh);
    if (st == HH_DONE || st == HH_THANKS) return;
    if (s_swipe_guard) return; /* acariciar al bebe no cambia de pagina */
    if (ui_pages_swipe(dir == LV_DIR_LEFT ? -1 : 1)) page_changed();
}

/* Boton del pais (arriba a la derecha): lleva a su pagina (mapa, distancia, hora); desde el
 * mapa, vuelve al bebe */
static void country_clicked(lv_event_t *e)
{
    (void)e;
    hh_state_t st = hh_state(&s_hh);
    if (st == HH_DONE || st == HH_THANKS) return;
    if (ui_pages_goto(ui_pages_current() == 1 ? 0 : 1)) page_changed();
}

/* La IncuNest del mapa: vuelve al bebe */
void ui_home_goto_baby(void)
{
    if (ui_pages_goto(UI_PAGE_BABY)) page_changed();
}

/* ----------------------------------------------------------------- copos */

#define SNOW_N 12
static lv_obj_t *s_snow[SNOW_N];
static bool s_snow_ok;  /* Inicio con el bebe a la vista, sin gesto ni llamada */
static bool s_snow_run;

static void snow_y_cb(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

/* Copos que caen despacio a los lados del bebe (cambio tienda): sin parpadeos */
static void snow_create(void)
{
    static const uint8_t X[SNOW_N] = { 16, 36, 24, 46, 12, 40, 186, 208, 196, 214, 180, 204 }; /* en la pantallita */
    for (int i = 0; i < SNOW_N; i++) {
        s_snow[i] = lv_label_create(s_scr);
        lv_label_set_text(s_snow[i], "*");
        lv_obj_set_style_text_font(s_snow[i], i % 3 ? &lv_font_es_16 : &lv_font_es_20, 0);
        lv_obj_set_style_text_color(s_snow[i], lv_color_hex(0x9EC2E6), 0);
        lv_obj_set_pos(s_snow[i], X[i], -20);
        lv_obj_set_clickable(s_snow[i], false);
        lv_obj_set_hidden(s_snow[i], true);
    }
}

static void snow_set(bool on)
{
    if (on == s_snow_run || !s_snow[0]) return;
    s_snow_run = on;
    for (int i = 0; i < SNOW_N; i++) {
        lv_obj_set_hidden(s_snow[i], !on);
        lv_anim_delete(s_snow[i], snow_y_cb);
        if (!on) continue;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_snow[i]);
        lv_anim_set_exec_cb(&a, snow_y_cb);
        lv_anim_set_values(&a, 40, ICON_Y - 22);
        lv_anim_set_duration(&a, 10000 + (uint32_t)(i * 517) % 6000);
        lv_anim_set_delay(&a, (uint32_t)(i * 1300) % 9000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

/* Navidad (24-dic .. 6-ene, con hora): guirnalda, copos y Papa Noel, puestos y conseguidos */
static void garland_apply(void)
{
    const rw_state_t *r = ui_rewards();
    int m, d;
    rewards_local_md(&m, &d);
    bool on = rw_wears(r, RW_ITEM_GARLAND, m, d);
    if (on == lv_obj_is_hidden(s_garland)) lv_obj_set_hidden(s_garland, !on);
    snow_set(s_snow_ok && rw_wears(r, RW_ITEM_SNOW, m, d));
    ui_outfit_apply();
}

/* Han cambiado las recompensas: halo en reposo, fondo, guirnalda, latido del boton y paginas */
void ui_home_rewards_apply(void)
{
    if (s_halo_mode == HALO_CALM) lv_obj_set_style_bg_color(s_halo, ui_theme_halo_calm(), 0);
    hh_state_t st = hh_state(&s_hh);
    if (st != HH_DONE && st != HH_THANKS) scr_bg_restore();
    garland_apply();
    row_apply(s_row_shown);
    ui_pages_rewards_apply();
}

/* "Volver al bebé" en el agradecimiento: cierra el gesto sin esperar */
static void back_btn_clicked(lv_event_t *e)
{
    (void)e;
    hh_dismiss(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* El boton prepara el gesto: el bebe abre los ojos y espera el dedo */
static bool s_incupray_btn; /* el boton dice «IncuPray»: entra en ella en vez del gesto */

static void hand_btn_clicked(lv_event_t *e)
{
    (void)e;
    if (s_incupray_btn) {
        ui_incupray_open();
        return;
    }
    info_hide();
    baby_react_cancel(); /* el gesto manda en los ojos */
    hh_button(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* ------------------------------------------------------- gesto: objetos */

/* corazon que sube y se desvanece: v 0..1000 */
static int32_t s_heart_base[N_HEARTS]; /* y de salida del vuelo de cada corazon */

static void heart_anim_cb(void *var, int32_t v)
{
    lv_obj_t *o = var;
    int32_t base = 0;
    for (int i = 0; i < N_HEARTS; i++) {
        if (s_hearts[i] == o) base = s_heart_base[i];
    }
    lv_obj_set_y(o, base - 40 * v / 1000);
    int32_t opa = v < 200 ? v * 255 / 200 : (1000 - v) * 255 / 800;
    lv_obj_set_style_image_opa(o, (lv_opa_t)(opa < 0 ? 0 : opa > 255 ? 255 : opa), 0);
}

static void hearts_run(bool on)
{
    for (int i = 0; i < N_HEARTS; i++) {
        lv_anim_delete(s_hearts[i], heart_anim_cb);
        lv_obj_set_hidden(s_hearts[i], !on);
        if (!on) continue;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_hearts[i]);
        lv_anim_set_exec_cb(&a, heart_anim_cb);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, 2400);
        lv_anim_set_delay(&a, (uint32_t)i * 600);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

static void target_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

static void target_run(bool on)
{
    lv_anim_delete(s_target, target_opa_cb);
    lv_obj_set_hidden(s_target, !on);
    if (!on) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_target);
    lv_anim_set_exec_cb(&a, target_opa_cb);
    lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_duration(&a, 700);
    lv_anim_set_reverse_duration(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static lv_obj_t *ring_circle(lv_obj_t *parent, int32_t d, bool filled)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, d, d);
    lv_obj_center(c);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(c, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(c, COL_CORAL, 0);
    lv_obj_set_style_border_width(c, filled ? 0 : 3, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    return c;
}

/* El anillo va debajo del bebe; diana, corazones, nombre y texto, encima.
 * Nada de esto es pulsable: los toques llegan al bebe y al boton. */
static void hold_create_below_baby(void)
{
    s_ring = lv_arc_create(s_scr);
    lv_obj_set_size(s_ring, RING_D, RING_D);
    lv_obj_set_pos(s_ring, BABY_CX - RING_D / 2, BABY_CY + 2 - RING_D / 2);
    lv_arc_set_rotation(s_ring, 270);
    lv_arc_set_bg_angles(s_ring, 0, 360);
    lv_arc_set_range(s_ring, 0, 1000);
    lv_arc_set_value(s_ring, 0);
    lv_obj_remove_style(s_ring, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(s_ring, false);
    lv_obj_set_style_arc_width(s_ring, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ring, COL_RING_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ring, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_ring, COL_CORAL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_ring, true, LV_PART_INDICATOR);
    lv_obj_set_hidden(s_ring, true);
}

static void hold_create_above_baby(void)
{
    s_target = lv_obj_create(s_scr);
    lv_obj_set_size(s_target, 52, 52);
    lv_obj_set_pos(s_target, BABY_CX - 26, BABY_CY + 30 - 26);
    lv_obj_set_style_bg_opa(s_target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_target, 0, 0);
    lv_obj_set_style_pad_all(s_target, 0, 0);
    lv_obj_set_scrollable(s_target, false);
    lv_obj_set_clickable(s_target, false);
    ring_circle(s_target, 48, false);
    ring_circle(s_target, 28, false);
    ring_circle(s_target, 10, true);
    lv_obj_set_hidden(s_target, true);

    static const int16_t HX[N_HEARTS] = { 50, 172, 160, 58 };
    static const int16_t HY[N_HEARTS] = { 112, 98, 170, 178 }; /* siguen al bebe (BABY_Y) */
    for (int i = 0; i < N_HEARTS; i++) {
        /* corazones de pixeles de 18x16 y 27x24 (antes la imagen al 60 y 75 %) */
        lv_obj_t *h = toy_heart_img(s_scr, TOY_HEART_FULL, i % 2 ? 2 : 3);
        int32_t hh = i % 2 ? 8 : 12;
        lv_obj_set_pos(h, HX[i] - hh - 1, HY[i] - hh);
        s_heart_base[i] = HY[i] - hh; /* salida del vuelo (heart_anim_cb) */
        lv_obj_set_style_image_opa(h, LV_OPA_TRANSP, 0);
        lv_obj_set_hidden(h, true);
        s_hearts[i] = h;
    }

    /* marco de la pantallita: encima del bebe, del anillo y de los corazones; debajo del nombre,
     * del texto del gesto y del agradecimiento */
    s_stage_frame = stage_part(true);

    /* al terminar el gesto: los papas con la IncuNest y el corazon, a pantalla completa. El gesto
     * ha terminado: el dedo ya no lo reanuda (pulsable solo para tapar al bebe de debajo); otro
     * empieza de cero con "Coge mi mano". "Volver al bebé" lo cierra sin esperar. */
    s_thanks_img = lv_image_create(s_scr);
    lv_image_set_src(s_thanks_img, &img_hand_thanks);
    lv_obj_set_pos(s_thanks_img, 0, 0);
    lv_obj_set_clickable(s_thanks_img, true);
    lv_obj_set_hidden(s_thanks_img, true);

    s_btn_back = lv_button_create(s_scr);
    lv_obj_set_size(s_btn_back, 150, ICON_D);
    lv_obj_align(s_btn_back, LV_ALIGN_TOP_MID, 0, ICON_Y);
    lv_obj_add_event_cb(s_btn_back, back_btn_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(s_btn_back);
    lv_label_set_text(lbl_back, tr(STR_HH_BACK));
    toy_button(s_btn_back, true);
    lv_obj_set_style_text_font(lbl_back, &lv_font_title_16, 0);
    lv_obj_center(lbl_back);
    lv_obj_set_hidden(s_btn_back, true);

    s_name = lv_label_create(s_scr);
    lv_label_set_text(s_name, "");
    lv_obj_set_style_text_font(s_name, &lv_font_title_16, 0);
    lv_obj_set_style_text_color(s_name, COL_INK, 0);
    toy_pill(s_name); /* etiqueta de juguete sobre la manta, como la app */
    lv_obj_set_style_pad_hor(s_name, 12, 0);
    lv_obj_set_style_pad_top(s_name, 2, 0);
    lv_obj_set_style_pad_bottom(s_name, 1, 0);
    lv_obj_set_style_max_width(s_name, 200, 0);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_name, LV_ALIGN_TOP_MID, 0, NAME_Y);
    lv_obj_set_hidden(s_name, true);
    lv_obj_add_flag(s_name, UI_FLAG_CLICK_SOUND); /* clicable solo sin nombre real (overlay_refresh) */
    lv_obj_add_event_cb(s_name, name_clicked_cb, LV_EVENT_CLICKED, NULL);
    ui_name_on_done(name_done);

    s_lbl_hold = lv_label_create(s_scr);
    lv_label_set_text(s_lbl_hold, "");
    lv_obj_set_size(s_lbl_hold, 224, HOLD_H);
    lv_obj_set_pos(s_lbl_hold, 8, ICON_Y);
    lv_obj_set_style_text_font(s_lbl_hold, &lv_font_title_14, 0);
    lv_obj_set_style_text_align(s_lbl_hold, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_lbl_hold, -2, 0); /* dos lineas en la fila de 36 */
    lv_obj_set_style_pad_hor(s_lbl_hold, 6, 0);
    lv_obj_set_style_pad_top(s_lbl_hold, 11, 0);
    lv_obj_set_style_radius(s_lbl_hold, TOY_R_CARD, 0);
    lv_obj_set_style_border_color(s_lbl_hold, COL_INK, 0);
    lv_obj_set_style_bg_color(s_lbl_hold, COL_CARD, 0);
    lv_label_set_long_mode(s_lbl_hold, LV_LABEL_LONG_MODE_WRAP); /* hasta dos lineas */
    lv_obj_set_hidden(s_lbl_hold, true);
}

static void hold_timer_cb(lv_timer_t *t)
{
    (void)t;
    /* un toque que empezo en el bebe y salio de el (PRESS_LOST) no manda release al bebe */
    if (s_swipe_guard && !s_aff_on) {
        bool down = false;
        for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
            down = down || lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED;
        }
        if (!down) s_swipe_guard = false;
    }
    hh_tick(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* Boton del pais en la barra de arriba (donde estaba la marca): bandera + nombre en una etiqueta de
 * juguete (crema, borde tinta, sombra dura) para que se vea que se puede tocar. Encima de las
 * paginas: tambien se ve (y se toca) en el mapa. */
static void country_button_create(void)
{
    s_country = lv_obj_create(s_scr);
    lv_obj_set_size(s_country, LV_SIZE_CONTENT, 26);
    toy_pill(s_country); /* se hunde al pulsarlo */
    lv_obj_set_style_pad_left(s_country, 4, 0);
    lv_obj_set_style_pad_right(s_country, 6, 0);
    lv_obj_set_style_pad_ver(s_country, 0, 0);
    lv_obj_set_style_pad_column(s_country, 4, 0);
    lv_obj_set_flex_flow(s_country, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_country, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(s_country, false);
    lv_obj_add_flag(s_country, UI_FLAG_CLICK_SOUND);
    lv_obj_set_ext_click_area(s_country, 6);
    lv_obj_set_gesture_bubble(s_country, false);
    lv_obj_add_event_cb(s_country, country_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_align(s_country, LV_ALIGN_TOP_RIGHT, -6, 5);
    s_country_flag = lv_image_create(s_country);
    lv_image_set_inner_align(s_country_flag, LV_IMAGE_ALIGN_STRETCH); /* bandera de 26 px a 16 */
    lv_obj_set_clickable(s_country_flag, false);
    s_country_lbl = lv_label_create(s_country);
    lv_label_set_text(s_country_lbl, "");
    lv_obj_set_style_text_font(s_country_lbl, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_country_lbl, COL_INK, 0);
    lv_obj_set_style_max_width(s_country_lbl, COUNTRY_LBL_W, 0);
    lv_label_set_long_mode(s_country_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_hidden(s_country, true);
}

/* "Coge mi mano" late mientras hoy no le hayas dado la mano: un anillo coral sale del boton y se
 * apaga, y descansa un poco antes del siguiente (v 0..1000). */
static void nudge_cb(void *var, int32_t v)
{
    lv_obj_t *b = var;
    lv_obj_set_style_outline_width(b, 1 + 6 * v / 1000, 0);
    lv_obj_set_style_outline_opa(b, (lv_opa_t)(190 * (1000 - v) / 1000), 0);
}

static void nudge_set(bool on)
{
    if (on == s_nudge) return;
    s_nudge = on;
    lv_anim_delete(s_btn_hand, nudge_cb);
    lv_obj_set_style_outline_width(s_btn_hand, 0, 0);
    lv_obj_set_style_outline_opa(s_btn_hand, LV_OPA_TRANSP, 0);
    if (!on) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_btn_hand);
    lv_anim_set_exec_cb(&a, nudge_cb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 1300);
    lv_anim_set_repeat_delay(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* Hoy aun no le has dado la mano (el corazon del dia esta por ganar): el ultimo dia con gesto de
 * las recompensas; sin hora, el corazon de hoy de la racha de acompañar; en la demo, los gestos de
 * esta demo. */
static bool today_pending(void)
{
    if (!s_have_last) return false;
    if (s_last.demo) return !ui_pages_demo_today();
    if (net_wifi_time_valid()) {
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        return ui_rewards()->last_day != rw_day_of(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
    return s_last.acc != TWIN_ACC_OFF && !(s_last.acc_week & 1u);
}

static void info_create(void)
{
    s_info = lv_label_create(s_scr);
    lv_label_set_text(s_info, "");
    lv_obj_set_width(s_info, 224);
    lv_obj_set_style_text_font(s_info, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_info, COL_NAVY_DARK, 0);
    lv_obj_set_style_text_align(s_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(s_info, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_info, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_info, TOY_BORDER, 0); /* borde del color del simbolo */
    lv_obj_set_style_radius(s_info, TOY_R_CARD, 0);
    toy_shadow(s_info, TOY_SHADOW_S);
    lv_obj_set_style_pad_hor(s_info, 10, 0);
    lv_obj_set_style_pad_ver(s_info, 7, 0);
    lv_label_set_long_mode(s_info, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_clickable(s_info, false);
    lv_obj_set_hidden(s_info, true);
}

/* ------------------------------------------------------------ sin WiFi */

static bool nowifi_girl(void)
{
    if (s_nowifi_force) return s_nowifi_force == 2;
    return s_have_last && s_last.inc.sex == TWIN_SEX_FEMALE;
}

static void nowifi_reapply(void)
{
    if (!s_have_last) return;
    twin_snapshot_t s = s_last;
    ui_home_apply(&s);
}

/* Decide el bebe sin WiFi con el ultimo estado. Devuelve true si ocupa el centro (bucle o
 * celebracion). Fuera de la demo; la celebracion solo con Inicio a la vista. */
static bool nowifi_update(void)
{
    if (!s_have_last) return false;
    if ((!s_last.link.wifi || s_nowifi_force) && !s_last.demo) {
        if (!s_nowifi_since) s_nowifi_since = lv_tick_get() | 1u;
        if (s_nowifi_force || lv_tick_elaps(s_nowifi_since) >= NOWIFI_DELAY_MS) ui_nowifi_show(s_nw, nowifi_girl());
    } else {
        s_nowifi_since = 0;
        if (ui_nowifi_active(s_nw) && !ui_nowifi_celebrating(s_nw)) {
            if (!s_last.demo && lv_screen_active() == s_scr) ui_nowifi_celebrate(s_nw, nowifi_girl(), nowifi_reapply);
            else ui_nowifi_hide(s_nw);
        }
    }
    return ui_nowifi_active(s_nw);
}

/* Consola `sinwifi`: hace como si no hubiera WiFi (sin esperar los 8 s); al soltarlo, si de
 * verdad hay WiFi, celebra */
void ui_home_nowifi_force(int8_t mode)
{
    s_nowifi_force = mode;
    nowifi_reapply();
}

/* Sin WiFi no llegan estados: el paso de los 8 s lo vigila este temporizador */
static void nowifi_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_have_last) return;
    bool before = ui_nowifi_active(s_nw);
    nowifi_update();
    if (ui_nowifi_active(s_nw) != before) nowifi_reapply();
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_home_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);
    s_bg_col = ui_theme_bg();

    /* imagenes a pantalla completa (debajo de todo) */
    s_empty_img = lv_image_create(s_scr);
    lv_image_set_src(s_empty_img, &img_incunest_empty);
    lv_obj_set_pos(s_empty_img, 0, 0); /* su dibujo (medido) ocupa y 46..275: ya cabe */
    lv_obj_set_hidden(s_empty_img, true);

    s_parents_img = lv_image_create(s_scr);
    lv_image_set_src(s_parents_img, &img_baby_parents);
    lv_obj_set_pos(s_parents_img, 0, PARENTS_Y);
    lv_obj_set_hidden(s_parents_img, true);

    s_stage_bg = stage_part(false); /* debajo del halo; el marco va encima del gesto */

    /* halo que respira */
    s_halo = lv_obj_create(s_scr);
    lv_obj_set_size(s_halo, 190, 190);
    lv_obj_set_style_radius(s_halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_halo, ui_theme_halo_calm(), 0);
    lv_obj_set_style_bg_opa(s_halo, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_halo, 0, 0);
    lv_obj_set_pos(s_halo, BABY_CX - 95, BABY_CY - 95);
    lv_obj_set_scrollable(s_halo, false);
    lv_obj_set_clickable(s_halo, false);
    halo_start_anim(2600);
    snow_create(); /* detras del bebe */

    hold_create_below_baby();

    lv_obj_t *baby = baby_widget_create(s_scr, BABY_X, BABY_Y);
    /* bebe sin WiFi: centrado, acaba en y 249; debajo su texto y los puntos de pagina (273) */
    s_nw = ui_nowifi_create(s_scr, (240 - NOWIFI_W) / 2, 249 - NOWIFI_H, 252);
    lv_timer_create(nowifi_timer_cb, NOWIFI_TICK_MS, NULL);
    lv_obj_set_clickable(baby, true);
    lv_obj_add_event_cb(baby, baby_touched, LV_EVENT_ALL, NULL);

    s_icon_thermo = state_icon_create(s_scr, 8, &img_icon_thermo);
    s_icon_photo = state_icon_create(s_scr, 8 + ICON_D + 6, &img_icon_lamp);
    lv_obj_t *tap[] = { s_icon_thermo, s_icon_photo };
    lv_event_cb_t tap_cb[] = { thermo_icon_clicked, photo_icon_clicked };
    for (int i = 0; i < 2; i++) { /* tocar: que es este tratamiento */
        lv_obj_set_clickable(tap[i], true);
        lv_obj_add_flag(tap[i], UI_FLAG_CLICK_SOUND); /* suenan como un boton */
        lv_obj_set_ext_click_area(tap[i], 4);
        lv_obj_add_event_cb(tap[i], tap_cb[i], LV_EVENT_CLICKED, NULL);
    }
    s_icon_heart = state_icon_create(s_scr, 8 + 2 * (ICON_D + 6), NULL); /* corazon de pixeles */
    lv_obj_set_clickable(s_icon_heart, true); /* tocar: oir su latido */
    lv_obj_add_flag(s_icon_heart, UI_FLAG_CLICK_SOUND);
    lv_obj_set_ext_click_area(s_icon_heart, 4);
    lv_obj_add_event_cb(s_icon_heart, heart_icon_clicked, LV_EVENT_CLICKED, NULL);

    s_btn_hand = lv_button_create(s_scr);
    lv_obj_set_size(s_btn_hand, HAND_W, ICON_D);
    lv_obj_set_pos(s_btn_hand, HAND_X, ICON_Y);
    lv_obj_set_style_pad_all(s_btn_hand, 0, 0);
    lv_obj_set_style_outline_color(s_btn_hand, COL_CORAL, 0); /* el latido de nudge_set */
    lv_obj_set_style_outline_pad(s_btn_hand, 1, 0);
    lv_obj_set_style_outline_opa(s_btn_hand, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(s_btn_hand, hand_btn_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_hand = lv_label_create(s_btn_hand);
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    toy_button(s_btn_hand, true); /* coral, borde tinta, sombra dura, letra Fredoka */
    lv_obj_set_style_text_align(s_lbl_hand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_lbl_hand, -3, 0);
    lv_obj_set_width(s_lbl_hand, HAND_W - 10);
    lv_label_set_long_mode(s_lbl_hand, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_center(s_lbl_hand);

    hold_create_above_baby();
    const hh_config_t hh_cfg = {
        .min_ms = CONFIG_INCUTWIN_HAND_MIN_S * 1000u,
        .grace_ms = CONFIG_INCUTWIN_HAND_GRACE_S * 1000u,
        .ready_ms = CONFIG_INCUTWIN_HAND_READY_S * 1000u,
        .thanks_ms = CONFIG_INCUTWIN_HAND_THANKS_S * 1000u,
        .cooldown_ms = CONFIG_INCUTWIN_HAND_COOLDOWN_S * 1000u,
    };
    hh_init(&s_hh, &hh_cfg);
    lv_timer_create(hold_timer_cb, HH_TICK_MS, NULL);

    ui_pages_create(s_scr); /* encima del bebe, debajo de las barras */
    info_create();
    lv_obj_add_event_cb(s_scr, scr_gesture, LV_EVENT_GESTURE, NULL);

    /* barra superior: marca a la izquierda (donde estaba el WiFi), pais a la derecha (donde
     * estaba la marca) y, en la demo, la etiqueta DEMO debajo de la marca */
    /* marca a media escala (144x26 -> 72x13): el gorro del bebe sube hasta y 11 y no cabe
     * debajo de la marca entera */
    s_garland = lv_image_create(s_scr); /* detras de la marca y del pais */
    lv_image_set_src(s_garland, &img_garland);
    lv_obj_set_pos(s_garland, 0, 0);
    lv_obj_set_clickable(s_garland, false);
    lv_obj_set_hidden(s_garland, true);
    lv_obj_t *wm = lv_image_create(s_scr);
    lv_image_set_src(wm, &img_wordmark);
    lv_image_set_inner_align(wm, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_set_size(wm, 72, 13);
    lv_obj_align(wm, LV_ALIGN_TOP_LEFT, 8, 13);
    country_button_create();

    s_demo_badge = lv_label_create(s_scr);
    lv_label_set_text(s_demo_badge, "DEMO");
    lv_obj_set_style_text_font(s_demo_badge, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_demo_badge, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_demo_badge, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(s_demo_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_demo_badge, 8, 0);
    lv_obj_set_style_pad_hor(s_demo_badge, 4, 0);
    lv_obj_set_style_pad_ver(s_demo_badge, 3, 0);
    lv_obj_set_pos(s_demo_badge, 8, 31);
    lv_obj_set_hidden(s_demo_badge, true);

    ui_streak_fx_create(s_scr); /* encima del agradecimiento, debajo de la barra */
    return s_scr;
}

/* Cualquier pulsacion en la pantalla: en la despedida del modo acompañar la cierra (pasados unos
 * segundos, por los toques fantasma del tactil). */
void ui_home_screen_pressed(void)
{
    ui_nowifi_skip(s_nw); /* un toque acaba la celebracion del WiFi */
    if (!s_farewell || lv_tick_elaps(s_farewell_ms) < FAREWELL_TOUCH_MIN_MS) return;
    s_farewell = false; /* un aviso por despedida */
    app_evt_acc_input_t in = APP_ACC_TOUCH;
    app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
}

void ui_home_incupray_set(bool open)
{
    s_incupray_btn = open;
    lv_label_set_text(s_lbl_hand, open ? "IncuPray" : tr(STR_HAND));
    lv_obj_set_hidden(s_btn_hand, !s_row_shown && !open);
}

void ui_home_retranslate(void)
{
    lv_label_set_text(s_lbl_hand, s_incupray_btn ? "IncuPray" : tr(STR_HAND));
    ui_pages_retranslate();
    ui_nowifi_retranslate(s_nw);
    hold_refresh(true);
}

/* ------------------------------------------------------------ state -> UI */

/* Fila de abajo: solo los simbolos del tratamiento en curso, juntos desde la izquierda, y el
 * boton "Coge mi mano". El corazon solo sale con un pulso normal (twin_apply filtra el resto). */
static void row_apply(bool shown)
{
    s_row_shown = shown;
    const twin_incubator_t *inc = &s_last.inc;
    bool baby = s_have_last && s_last.show_baby;
    bool on[] = { baby && inc->thermo != TWIN_THERMO_OFF, baby && inc->photo, baby && inc->bpm > 0 };
    lv_obj_t *icons[] = { s_icon_thermo, s_icon_photo, s_icon_heart };
    int32_t x = 8;
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        lv_obj_set_hidden(icons[i], !shown || !on[i]);
        if (on[i]) {
            lv_obj_set_x(icons[i], x);
            x += ICON_D + 6;
        }
    }
    lv_obj_set_hidden(s_btn_hand, !shown && !s_incupray_btn); /* la IncuPray se ve aunque no haya bebe */
    nudge_set(shown && today_pending() && !s_incupray_btn);
}

static void home_set_view(home_view_t view)
{
    s_view = view;
    home_bg_apply();
    baby_set_shown(view == VIEW_BABY); /* VIEW_NOWIFI: lo pinta ui_nowifi */
    /* durante el gesto la fila la ocupa su texto */
    row_apply(view == VIEW_BABY && hh_state(&s_hh) == HH_IDLE);
    lv_obj_set_hidden(s_empty_img, view != VIEW_EMPTY);
    lv_obj_set_hidden(s_parents_img, view != VIEW_PARENTS);
}

/* Con sus papas: la familia de piel oscura para los tonos oscuros (3..4); con piel clara,
 * la escena de nina si el bebe es nina. Aun no hay familia oscura con nina: los tonos
 * oscuros usan la de siempre aunque sea nina (el bebe animado si cambia). */
static void parents_img_apply(uint8_t skin, bool girl)
{
    const void *src = skin >= TWIN_SKIN_DARK_MIN ? &img_baby_parents_black
                    : girl                       ? &img_baby_parents_girl
                                                 : &img_baby_parents;
    if (lv_image_get_src(s_parents_img) != src) {
        lv_image_set_src(s_parents_img, src);
        lv_obj_set_y(s_parents_img, src == &img_baby_parents_black ? PARENTS_BLACK_Y : PARENTS_Y);
    }
}

/* ---------------------------------------------------- gesto -> pantalla */

static void label_set(lv_obj_t *l, const char *t)
{
    if (strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}

/* Texto del gesto: una o dos lineas, centrado en vertical en la caja */
static void hold_text(const char *t)
{
    if (strcmp(lv_label_get_text(s_lbl_hold), t) == 0) return;
    lv_label_set_text(s_lbl_hold, t);
    lv_point_t sz;
    lv_text_get_size(&sz, t, &lv_font_title_14, 0, -2, 224 - 12, LV_TEXT_FLAG_NONE);
    int32_t pad = (HOLD_H - sz.y) / 2 - 2;
    lv_obj_set_style_pad_top(s_lbl_hold, pad < 0 ? 0 : pad, 0);
}

static void thanks_opa_cb(void *var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0); }

/* Imagen final del gesto: aparece fundiendose */
static void thanks_show(bool on)
{
    if (on != lv_obj_has_flag(s_thanks_img, LV_OBJ_FLAG_HIDDEN)) return;
    lv_anim_delete(s_thanks_img, thanks_opa_cb);
    lv_obj_set_hidden(s_thanks_img, !on);
    lv_obj_set_hidden(s_btn_back, !on);
    if (!on) return;
    lv_obj_set_style_opa(s_thanks_img, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_thanks_img);
    lv_anim_set_exec_cb(&a, thanks_opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, 450);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* Anillo por vueltas: la vuelta 0 se llena sobre el fondo rosa; cada vuelta siguiente se
 * pinta encima de la anterior, ya completa, en el color siguiente. */
static void ring_apply(uint32_t held_ms, bool grace)
{
    uint32_t min = (uint32_t)CONFIG_INCUTWIN_HAND_MIN_S * 1000u;
    if (min == 0) min = 1;
    int32_t lap = (int32_t)(held_ms / min);
    uint32_t in = held_ms % min;
    if (lap != s_ring_lap) {
        s_ring_lap = lap;
        lv_obj_set_style_arc_color(s_ring,
                                   lap == 0 ? COL_RING_BG : lv_color_hex(LAP_COLS[(lap - 1) % N_LAP_COLS]),
                                   LV_PART_MAIN);
    }
    lv_obj_set_style_arc_color(s_ring, grace ? COL_RING_DIM : lv_color_hex(LAP_COLS[lap % N_LAP_COLS]),
                               LV_PART_INDICATOR);
    lv_arc_set_value(s_ring, (int32_t)((uint64_t)in * 1000u / min));
}

/* Pregunta por el nombre del bebe recien llegado cuando no molesta: bebe dentro y a la vista,
 * sin nombre real, fuera de la demo, "Coge mi mano" en reposo, sin alarma y en la pagina del
 * bebe (spec nombre-bebe-panel). */
static void name_maybe_ask(void)
{
    if (!s_have_last || ui_name_open() || !baby_name_should_ask()) return;
    if (!bn_can_ask(&s_last) || hh_state(&s_hh) != HH_IDLE || ui_pages_current() != 0) return;
    ui_name_ask();
}

static void name_clicked_cb(lv_event_t *e)
{
    (void)e;
    ui_name_edit(); /* la etiqueta solo es clicable sin nombre real y fuera de la demo */
}

static void name_done(void) { overlay_refresh(); }

/* Nombre y pais: con el bebe a la vista, salvo mientras se agradece (imagen final) */
static void overlay_refresh(void)
{
    if (!s_have_last) return;
    const twin_snapshot_t *s = &s_last;
    bool thanks = hh_state(&s_hh) == HH_THANKS;
    /* nombre real (con consentimiento) > el que le ha puesto el usuario > invitacion a ponerlo
     * (spec nombre-bebe-panel); en la demo solo el real */
    const char *name = s->inc.name;
    bool can_name = !s->demo && s->inc.name[0] == '\0';
    if (can_name) {
        name = baby_name_local();
        if (!name[0] && s->inc.baby == TWIN_BABY_IN) name = tr(STR_NAME_PLACEHOLDER);
    }
    bool show_name = s->show_baby && name[0] != '\0' && !thanks && !ui_nowifi_active(s_nw);
    if (show_name) label_set(s_name, name);
    lv_obj_set_hidden(s_name, !show_name);
    lv_obj_set_clickable(s_name, can_name && hh_state(&s_hh) == HH_IDLE); /* no a mitad del gesto */

    /* boton del pais arriba: siempre que se sepa, salvo en la despedida y el final del gesto */
    bool show_country = s->inc.country[0] != '\0' && !ui_home_overlay_active();
    if (show_country) {
        label_set(s_country_lbl, tr_country(s->inc.country));
        const lv_image_dsc_t *flag = flag_image(s->inc.country);
        lv_obj_set_hidden(s_country_flag, flag == NULL);
        if (flag && lv_image_get_src(s_country_flag) != flag) {
            lv_image_set_src(s_country_flag, flag);
            lv_obj_set_size(s_country_flag, (int32_t)flag->header.w * 16 / flag->header.h, 16);
        }
    }
    lv_obj_set_hidden(s_country, !show_country);
}

static void scr_bg_restore(void)
{
    home_bg(s_halo_mode == HALO_PHOTO ? COL_BG_PHOTO : ui_theme_bg());
}

/* Pinta el estado del gesto (spec twin-display, "Coge mi mano en pantalla"). Los cambios de
 * estado rehacen visibilidad, animaciones y ojos; cada tick solo mueve anillo y textos. */
static void hold_refresh(bool force)
{
    static bool shown_view;
    uint32_t now = lv_tick_get();
    hh_state_t st = hh_state(&s_hh);
    bool idle = st == HH_IDLE;
    bool baby_view = s_have_last && s_last.show_baby && ui_pages_current() == 0;

    if (hh_take_send(&s_hh)) app_events_post(TWIN_EVT_HAND_HOLD, NULL, 0);

    if (s_tel_gesture) {
        if (!idle) {
            s_tel_ms = hh_held_ms(&s_hh, now);
        } else { /* fin de la mano (soltar, cambio de pantalla, demo...): el evento sale una vez */
            char d[96];
            snprintf(d, sizeof d, "{\"sec\":%lu,\"inc\":\"%s\",\"super\":%d}", (unsigned long)(s_tel_ms / 1000),
                     s_tel_inc, s_super_sent ? 1 : 0);
            telemetry_log("gesture", d);
            s_tel_gesture = false;
        }
    }

    if (st == HH_DONE && s_hh_shown == HH_HOLDING) {
        /* primer gesto del dia: un corazon vuela a la racha (acompañar o demo) */
        bool demo = s_have_last && s_last.demo;
        uint8_t week = demo ? ui_pages_demo_week() : s_last.acc_week;
        if (today_pending() && baby_view) {
            ui_streak_fx_play(week, demo ? 0 : s_last.acc_super, demo ? 0 : s_last.acc_rest, BABY_CX,
                              BABY_CY + 30, 1);
        }
        if (demo) rewards_demo_gesture(s_last.inc.country); /* el saldo simulado de la demo */
        ui_pages_note_hand(); /* +1 en los logros */
        if (s_have_last && !s_last.demo) { /* modo acompañar: hoy cuenta (si esta apagado, nada) */
            app_evt_acc_input_t in = APP_ACC_GESTURE;
            app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
            if (s_last.acc != TWIN_ACC_OFF) {
                s_tel_gesture = true;
                s_tel_ms = hh_held_ms(&s_hh, now);
                s_tel_inc[0] = '\0'; /* id solo si es apto para JSON sin escapar */
                const char *id = s_last.link.paired ? s_last.link.incubator_id : "";
                size_t i = 0;
                for (; id[i] && i + 1 < sizeof s_tel_inc; i++) {
                    char c = id[i];
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                          c == '_' || c == '-')) break;
                    s_tel_inc[i] = c;
                }
                s_tel_inc[id[i] ? 0 : i] = '\0';
            }
        }
    }
    if (force || (int)st != s_hh_shown || baby_view != shown_view) {
        s_hh_shown = (int)st;
        shown_view = baby_view;
        row_apply(baby_view && idle);
        if (!baby_view || !idle) info_hide();
        lv_obj_set_hidden(s_lbl_hold, !baby_view || idle);
        lv_obj_set_hidden(s_ring, !baby_view || idle || st == HH_READY);
        target_run(baby_view && st == HH_READY);
        hearts_run(baby_view && (st == HH_HOLDING || st == HH_DONE));
        thanks_show(baby_view && st == HH_THANKS);
        home_bg_apply(); /* sin pantallita en el agradecimiento */
        overlay_refresh();
        nav_apply();
        if (idle || !baby_view) ui_streak_fx_hide();
        if (idle) {
            s_ring_lap = -1;
            s_super_sent = false;
            name_maybe_ask(); /* si llego un bebe durante el gesto, se pregunta al acabar */
        }

        bool boxed = st == HH_READY || st == HH_THANKS;
        lv_obj_set_style_bg_opa(s_lbl_hold, boxed ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_lbl_hold, boxed ? TOY_BORDER : 0, 0);
        toy_shadow(s_lbl_hold, boxed ? TOY_SHADOW_S : 0);
        lv_obj_set_style_text_color(s_lbl_hold,
                                    (st == HH_READY || st == HH_GRACE) ? COL_CORAL : COL_INK, 0); /* coral de siempre (Pablo, 27-nov) */
        if (baby_view && (st == HH_DONE || st == HH_THANKS)) {
            home_bg(COL_BG_THANKS);
        } else {
            scr_bg_restore();
        }
        if (idle) { /* fin del gesto: vuelve el estado real */
            baby_set_awake(s_awake_real);
        } else { /* listo y gracia: te espera con los ojos abiertos; con tu dedo, los cierra */
            baby_set_awake(st == HH_READY || st == HH_GRACE);
        }
    }
    if (idle || !baby_view) return;

    uint16_t p = hh_progress_permille(&s_hh, now);
    ring_apply(hh_held_ms(&s_hh, now), st == HH_GRACE);
    /* Sin barra de estado: lo que contaba ella (segundos de espera, super corazon) va en la
     * segunda linea del texto del gesto */
    char buf[96];
    switch (st) {
    case HH_READY:
        hold_text(tr(STR_HH_READY));
        break;
    case HH_HOLDING:
        hold_text(tr(p < 500 ? STR_HH_HOLD1 : STR_HH_HOLD2));
        break;
    case HH_GRACE: {
        int n = snprintf(buf, sizeof(buf), "%s\n", tr(STR_HH_GRACE));
        if (n > 0 && n < (int)sizeof(buf)) {
            snprintf(buf + n, sizeof(buf) - n, tr(STR_HH_ST_GRACE), (unsigned)hh_grace_left_s(&s_hh, now));
        }
        hold_text(buf);
        break;
    }
    default: /* completado, con o sin dedo */
        if (st == HH_DONE && s_last.acc != TWIN_ACC_OFF && !s_last.demo) {
            /* modo acompañar: sigue con el dedo hasta los 3 min y es un super corazon */
            uint32_t held_s = hh_held_ms(&s_hh, now) / 1000;
            int n = snprintf(buf, sizeof(buf), "%s\n", tr(STR_HH_THANKS));
            if (held_s >= ACC_SUPER_S) {
                if (!s_super_sent) {
                    s_super_sent = true;
                    if (ui_rewards()->super_day != ui_rewards()->last_day) ui_streak_fx_bonus(2);
                    app_evt_acc_input_t in = APP_ACC_SUPER;
                    app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
                }
                if (n > 0 && n < (int)sizeof(buf)) snprintf(buf + n, sizeof(buf) - n, "%s", tr(STR_HH_ST_SUPER));
            } else if (n > 0 && n < (int)sizeof(buf)) {
                unsigned left = ACC_SUPER_S - held_s;
                snprintf(buf + n, sizeof(buf) - n, tr(STR_HH_ST_SUPER_IN), left / 60, left % 60);
            }
            hold_text(buf);
        } else {
            hold_text(tr(STR_HH_THANKS));
        }
        break;
    }
}

/* La barra no sale en la despedida del modo acompañar ni en el final del gesto (completado con el
 * dedo puesto y el agradecimiento a pantalla completa): spec hmi-navegacion-avisos, "la barra no
 * aparece en ... la despedida". Oculta, tampoco se puede pulsar: un toque no cierra la despedida y
 * a la vez cambia de pantalla. */
bool ui_home_overlay_active(void)
{
    hh_state_t st = hh_state(&s_hh);
    bool baby_view = s_have_last && s_last.show_baby && ui_pages_current() == 0;
    return (s_have_last && s_last.acc == TWIN_ACC_FAREWELL) || (baby_view && (st == HH_DONE || st == HH_THANKS));
}

bool ui_home_gesture_idle(void) { return hh_state(&s_hh) == HH_IDLE; }

/* La barra es comun a todas las pantallas (ui_nav.c): se esconde al momento, sin esperar al
 * siguiente refresco de ui.c */
static void nav_apply(void)
{
    ui_nav_update();
}

void ui_home_apply(const twin_snapshot_t *s)
{
    const twin_incubator_t *inc = &s->inc;
    s_awake_real = inc->awake;
    s_last = *s;
    s_have_last = true;
    if (!s->demo) baby_name_observe(s); /* la demo no es una estancia de verdad */
    /* aviso o teclado abiertos y el estado ha cambiado (alarma, demo, otro bebe): se cierran
     * sin contar como preguntado */
    if (ui_name_open() && bn_must_close(s, ui_name_open_key(), baby_name_key())) ui_name_close();
    bool farewell = s->acc == TWIN_ACC_FAREWELL;
    if (farewell && !s_farewell) s_farewell_ms = lv_tick_get();
    s_farewell = farewell;
    nav_apply();
    ui_pages_apply(s);
    hh_set_available(&s_hh, s->show_baby && ui_pages_current() == 0,
                     !s->demo && s->link.broker_connected && s->link.paired, lv_tick_get());
    if (s_hb_timer && !(s->show_baby && inc->bpm > 0)) heart_sound_set(false);
    bool hh_idle = hh_state(&s_hh) == HH_IDLE;

    lv_obj_set_hidden(s_demo_badge, !s->demo);
    bool nowifi = nowifi_update();
    home_set_view(nowifi            ? VIEW_NOWIFI
                  : s->parents_view ? VIEW_PARENTS
                  : s->show_baby    ? VIEW_BABY
                                    : VIEW_EMPTY);
    overlay_refresh();
    garland_apply(); /* la temporada empieza y acaba a medianoche */
    /* El sexo lo decide ThingsBoard (Contrato v0.2 §3.4.1); desconocido = nino, el dibujo
     * de siempre. Afecta al bebe animado y a la escena "con sus papas" / "en casa". */
    bool girl = inc->sex == TWIN_SEX_FEMALE;
    parents_img_apply(inc->skin, girl);

    lv_color_t tcol = inc->thermo == TWIN_THERMO_HEATING ? COL_WARM : COL_OK;
    treat_icon_set(s_icon_thermo, s->show_baby && inc->thermo != TWIN_THERMO_OFF, tcol);
    treat_icon_set(s_icon_photo, s->show_baby && inc->photo, COL_PHOTO);
    row_apply(s_row_shown); /* el tratamiento puede haber cambiado */

    baby_set_skin_tone(inc->skin);
    ui_nowifi_set_skin(inc->skin); /* el bebe de la antena, con la piel del bebe */
    baby_set_girl(girl);
    if (hh_idle) baby_set_awake(inc->awake); /* durante el gesto los ojos los pone el gesto */
    baby_set_heart_rate(s->show_baby ? inc->bpm : 0);

    bool calling = s->acc == TWIN_ACC_WAITING || (s->acc != TWIN_ACC_OFF && s->acc_remind);
    if (!s->show_baby || nowifi)                halo_set_mode(HALO_OFF);
    else if (calling)                            halo_set_mode(HALO_CALL);
    else if (inc->photo)                         halo_set_mode(HALO_PHOTO);
    else if (inc->thermo == TWIN_THERMO_HEATING) halo_set_mode(HALO_WARM);
    else                                         halo_set_mode(HALO_CALM);
    s_snow_ok = s->show_baby && !nowifi && !calling && hh_idle && ui_pages_current() == 0;
    garland_apply();

    /* el modo del halo puede haber repintado el fondo: el gesto manda sobre el */
    hold_refresh(false);
    if (!hh_idle && s->show_baby && (hh_state(&s_hh) == HH_DONE || hh_state(&s_hh) == HH_THANKS)) {
        home_bg(COL_BG_THANKS);
    }
    name_maybe_ask();
}

/* ------------------------------------------------------- consola (ui_debug) */

/* Revisar el estilo: los pasos del gesto sin tocar la pantalla. Empezar el gesto o poner el dedo
 * solo cuando el boton «Coge mi mano» de verdad estaria a la vista (bebe, pagina del bebe, sin
 * IncuPray): asi la consola no puede contar un gesto sin bebe. Ojo: con bebe, un gesto completado
 * desde la consola cuenta como uno de verdad (corazon, racha, telemetria). */
bool ui_home_debug_hand(int step)
{
    bool shown = s_have_last && s_last.show_baby && s_view == VIEW_BABY && ui_pages_current() == UI_PAGE_BABY &&
                 !s_incupray_btn;
    if (step <= 1 && !shown) return false;
    uint32_t now = lv_tick_get();
    switch (step) {
    case 0: info_hide(); hh_button(&s_hh, now); break;
    case 1: hh_touch(&s_hh, true, now); break;
    case 2: hh_touch(&s_hh, false, now); break;
    default: { /* cierra el gesto en cualquier paso, como al salir de la pagina del bebe */
        bool can_send = s_have_last && !s_last.demo && s_last.link.broker_connected && s_last.link.paired;
        hh_set_available(&s_hh, false, can_send, now);
        break;
    }
    }
    hold_refresh(false);
    return true;
}

/* Cambia de pagina como el dedo o el boton del pais (fuera burbuja y latido, y repintar) */
void ui_home_debug_page(int page)
{
    if (ui_pages_goto(page)) page_changed();
}
