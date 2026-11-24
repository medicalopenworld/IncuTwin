/* Pantalla principal (spec twin-display). Layout vertical 240x320:
 *   barra superior : WiFi | marca | engranaje
 *   centro         : bebe + halo | incubadora vacia | con sus papas (dos tonos de piel)
 *   cuerpo del bebe: pais de la incubadora, si se comparte (oculto durante el gesto)
 *   fila           : simbolos del tratamiento en curso (solo los activos, juntos a la
 *                    izquierda) + boton "Coge mi mano" (o el texto del gesto)
 *   abajo          : barra de estado siempre visible
 * En demo se desliza a izquierda/derecha entre tres paginas (ui_pages.c).
 * El latido solo suena al tocar el icono del corazon (ritmo real del bebe).
 * El gesto "Coge mi mano" (spec hold-my-hand) lo lleva hand_hold; aqui solo se pinta. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acc_logic.h"
#include "assets.h"
#include "baby_widget.h"
#include "hand_hold.h"
#include "sdkconfig.h"
#include "sound.h"
#include "storage.h"
#include "twin_logic.h"
#include "ui_internal.h"
#include "ui_onboarding.h"
#include "ui_pages.h"
#include "usage_stats.h"

#define BABY_X 63   /* sprite de 114x185 */
#define BABY_Y 38
#define BABY_CX (BABY_X + 57)
#define BABY_CY (BABY_Y + 92)
#define ICON_D 44
#define ICON_Y 232
#define STATUS_Y 288

/* gesto "Coge mi mano" */
#define RING_D        196
#define NAME_Y        206
#define COUNTRY_Y     164 /* centrado en la manta, entre la cara y el nombre */
#define N_HEARTS      4
#define HH_TICK_MS    100
#define COL_RING_BG   lv_color_hex(0xF5C4B3)
#define COL_RING_DIM  lv_color_hex(0xEBA8A0)
#define COL_BG_THANKS lv_color_hex(0xFBEDEA)
#define HOLD_H        40
/* Vueltas del anillo: cada minuto extra pinta otra vuelta encima, en el color siguiente.
 * Colores muy distintos entre vueltas seguidas para que se vea el cambio:
 * coral -> azul de la marca -> ambar -> verde -> coral... */
static const uint32_t LAP_COLS[] = { 0xDE6F63, 0x054E92, 0xF5A93B, 0x3F9E63 };
#define N_LAP_COLS (sizeof(LAP_COLS) / sizeof(LAP_COLS[0]))

typedef enum { HALO_OFF = 0, HALO_CALM, HALO_WARM, HALO_PHOTO, HALO_ALARM, HALO_CALL } halo_mode_t;
typedef enum { VIEW_EMPTY = 0, VIEW_BABY, VIEW_PARENTS } home_view_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_halo, *s_empty_img, *s_parents_img, *s_wifi_btn, *s_wifi_img, *s_demo_badge;
static lv_obj_t *s_info;          /* burbuja con la explicacion del simbolo tocado */
static lv_timer_t *s_info_timer;
static lv_obj_t *s_icon_thermo, *s_icon_photo, *s_icon_heart;
static lv_obj_t *s_btn_hand, *s_lbl_hand;
static lv_obj_t *s_btn_back; /* "Volver al bebé" en el agradecimiento */
static lv_obj_t *s_status, *s_lbl_status;
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
static twin_snapshot_t s_last;  /* para restaurar la barra de estado */
static bool s_have_last;
static bool s_row_shown; /* la fila se ve: bebe a la vista y sin gesto */
static uint32_t s_farewell_ms; /* lv_tick al entrar en la despedida del modo acompañar */
static bool s_farewell;
#define FAREWELL_TOUCH_MIN_MS 5000 /* que no la cierre un toque fantasma nada mas salir */

static void hold_refresh(bool force);
static void overlay_refresh(void);

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
    lv_color_t col = COL_HALO_CALM;
    uint32_t period = 2600;
    switch (m) {
    case HALO_OFF:
        lv_obj_set_hidden(s_halo, true);
        lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
        return;
    case HALO_WARM:  col = COL_HALO_WARM; break;
    case HALO_PHOTO: col = COL_HALO_PHOTO; period = 2000; break;
    case HALO_ALARM: col = COL_HALO_ALARM; period = 700; break;
    case HALO_CALL:  col = COL_HALO_CALL; period = 1200; break;
    default: break;
    }
    lv_obj_set_hidden(s_halo, false);
    lv_obj_set_style_bg_color(s_halo, col, 0);
    lv_obj_set_style_bg_color(s_scr, m == HALO_PHOTO ? COL_BG_PHOTO : COL_BG, 0);
    halo_start_anim(period);
}

/* ----------------------------------------------------------- state icons */

static lv_obj_t *state_icon_create(lv_obj_t *parent, int32_t x, const void *src)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, ICON_D, ICON_D);
    lv_obj_set_pos(c, x, ICON_Y);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_t *icon = lv_image_create(c);
    lv_image_set_src(icon, src);
    lv_obj_center(icon);
    return c;
}

static void state_icon_set(lv_obj_t *c, lv_color_t col) { lv_obj_set_style_border_color(c, col, 0); }

/* Tratamiento: activo = circulo relleno de su color con el icono en blanco; inactivo = atenuado */
static void treat_icon_set(lv_obj_t *c, bool on, lv_color_t col)
{
    lv_obj_t *icon = lv_obj_get_child(c, 0);
    lv_obj_set_style_border_color(c, on ? col : COL_OFFLINE, 0);
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
    case TWIN_THERMO_ALARM:   info_show(tr(STR_INFO_TEMP_ALARM), COL_RED); break;
    default:                  info_show(tr(STR_INFO_STABLE), COL_OK); break;
    }
}

static void photo_icon_clicked(lv_event_t *e)
{
    (void)e;
    info_show(tr(STR_INFO_PHOTO), COL_PHOTO);
}

/* ------------------------------------------------------------- handlers */

static void gear_clicked(lv_event_t *e)
{
    (void)e;
    ui_go_settings();
}

static void reset_confirmed(lv_event_t *e)
{
    (void)e;
    storage_factory_reset();
}

static void gear_long_pressed(lv_event_t *e)
{
    (void)e;
    ui_msgbox("IncuTwin", tr(STR_RESET_Q), tr(STR_RESET), COL_RED, reset_confirmed, NULL);
}

/* Icono WiFi -> cambiar de red (spec onboarding) */
static void wifi_change_confirmed(lv_event_t *e)
{
    lv_obj_t *mb = lv_event_get_user_data(e);
    lv_msgbox_close(mb);
    ui_onboarding_start_wifi_only();
}

static void wifi_icon_clicked(lv_event_t *e)
{
    (void)e;
    ui_msgbox("WiFi", tr(STR_WIFI_CHANGE_Q), tr(STR_WIFI_CHANGE), COL_NAVY, wifi_change_confirmed, NULL);
}

/* "Coge mi mano": el dedo sobre el bebe sostiene el gesto y cuenta uso */
static void baby_touched(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED && hh_state(&s_hh) == HH_IDLE) {
        info_show(tr(STR_INFO_PRESS_HAND), COL_CORAL); /* el gesto empieza con el boton */
        return;
    }
    if (c == LV_EVENT_PRESSED) {
        info_hide();
        usage_hand_set(true);
        hh_touch(&s_hh, true, lv_tick_get());
        hold_refresh(false);
    } else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
        usage_hand_set(false);
        hh_touch(&s_hh, false, lv_tick_get());
        hold_refresh(false);
    }
}

static void heart_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_hb_timer = NULL; /* el timer de una vez se borra solo */
    sound_heartbeat(false);
    lv_obj_set_style_bg_color(s_icon_heart, COL_CARD, 0);
}

/* Icono del corazon: suena el latido al ritmo real del bebe 10 s; otro toque lo para */
static void heart_sound_set(bool on)
{
    if (s_hb_timer) {
        lv_timer_delete(s_hb_timer);
        s_hb_timer = NULL;
    }
    sound_heartbeat(on);
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
        char buf[128];
        int n = snprintf(buf, sizeof(buf), tr(STR_INFO_HEART), (unsigned)s_last.inc.bpm);
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
    if (ui_pages_swipe(dir == LV_DIR_LEFT ? -1 : 1)) page_changed();
}

/* Etiqueta del pais: lleva a su pagina (mapa, distancia, hora) */
static void country_clicked(lv_event_t *e)
{
    (void)e;
    hh_state_t st = hh_state(&s_hh);
    if (st == HH_DONE || st == HH_THANKS) return;
    if (ui_pages_goto(1)) page_changed();
}

static void status_bar_apply(const twin_snapshot_t *s);

void ui_home_status_refresh(void)
{
    if (s_have_last && hh_state(&s_hh) == HH_IDLE) status_bar_apply(&s_last);
}

/* Etiqueta del pais en la pagina del mapa: vuelve al bebe */
void ui_home_goto_baby(void)
{
    if (ui_pages_goto(0)) page_changed();
}

/* "Volver al bebé" en el agradecimiento: cierra el gesto sin esperar */
static void back_btn_clicked(lv_event_t *e)
{
    (void)e;
    hh_dismiss(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* El boton prepara el gesto: el bebe abre los ojos y espera el dedo */
static void hand_btn_clicked(lv_event_t *e)
{
    (void)e;
    info_hide();
    hh_button(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* ------------------------------------------------------- gesto: objetos */

/* corazon que sube y se desvanece: v 0..1000 */
static void heart_anim_cb(void *var, int32_t v)
{
    lv_obj_t *o = var;
    int32_t base = (int32_t)(intptr_t)lv_obj_get_user_data(o);
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
    static const int16_t HY[N_HEARTS] = { 110, 96, 168, 176 };
    for (int i = 0; i < N_HEARTS; i++) {
        lv_obj_t *h = lv_image_create(s_scr);
        lv_image_set_src(h, &img_heart);
        lv_image_set_scale(h, i % 2 ? 150 : 190); /* 256 = 100 % */
        lv_obj_set_pos(h, HX[i] - 17, HY[i] - 15);
        lv_obj_set_user_data(h, (void *)(intptr_t)(HY[i] - 15));
        lv_obj_set_style_image_opa(h, LV_OPA_TRANSP, 0);
        lv_obj_set_hidden(h, true);
        s_hearts[i] = h;
    }

    /* al terminar el gesto: los papas con la IncuNest y el corazon, a pantalla completa. El gesto
     * ha terminado: el dedo ya no lo reanuda (pulsable solo para tapar al bebe de debajo); otro
     * empieza de cero con "Coge mi mano". "Volver al bebé" lo cierra sin esperar. */
    s_thanks_img = lv_image_create(s_scr);
    lv_image_set_src(s_thanks_img, &img_hand_thanks);
    lv_obj_set_pos(s_thanks_img, 0, 0);
    lv_obj_set_clickable(s_thanks_img, true);
    lv_obj_set_hidden(s_thanks_img, true);

    s_btn_back = lv_button_create(s_scr);
    lv_obj_set_size(s_btn_back, 132, ICON_D);
    lv_obj_align(s_btn_back, LV_ALIGN_TOP_MID, 0, ICON_Y);
    lv_obj_set_style_bg_color(s_btn_back, COL_CORAL, 0);
    lv_obj_set_style_radius(s_btn_back, 14, 0);
    lv_obj_set_style_shadow_width(s_btn_back, 0, 0);
    lv_obj_add_event_cb(s_btn_back, back_btn_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(s_btn_back);
    lv_obj_set_style_text_font(lbl_back, &lv_font_es_14, 0);
    lv_label_set_text(lbl_back, tr(STR_HH_BACK));
    lv_obj_center(lbl_back);
    lv_obj_set_hidden(s_btn_back, true);

    s_name = lv_label_create(s_scr);
    lv_label_set_text(s_name, "");
    lv_obj_set_style_text_font(s_name, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(s_name, COL_NAVY_DARK, 0);
    lv_obj_set_style_bg_color(s_name, COL_CARD, 0);
    lv_obj_set_style_bg_opa(s_name, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_name, COL_CORAL, 0);
    lv_obj_set_style_border_width(s_name, 2, 0);
    lv_obj_set_style_radius(s_name, 13, 0);
    lv_obj_set_style_pad_hor(s_name, 12, 0);
    lv_obj_set_style_pad_ver(s_name, 2, 0);
    lv_obj_set_style_max_width(s_name, 200, 0);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_name, LV_ALIGN_TOP_MID, 0, NAME_Y);
    lv_obj_set_hidden(s_name, true);

    /* pais: bandera + nombre sobre el cuerpo del bebe; es un boton que lleva a la pagina del pais */
    s_country = lv_obj_create(s_scr);
    lv_obj_set_size(s_country, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_bg_color(s_country, COL_CARD, 0);
    lv_obj_set_style_bg_color(s_country, COL_BG_PHOTO, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_country, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_country, COL_NAVY, 0);
    lv_obj_set_style_border_width(s_country, 2, 0);
    lv_obj_set_style_radius(s_country, 8, 0);
    lv_obj_set_style_pad_left(s_country, 4, 0);
    lv_obj_set_style_pad_right(s_country, 7, 0);
    lv_obj_set_style_pad_ver(s_country, 0, 0);
    lv_obj_set_style_pad_column(s_country, 5, 0);
    lv_obj_set_flex_flow(s_country, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_country, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(s_country, false);
    lv_obj_add_flag(s_country, UI_FLAG_CLICK_SOUND);
    lv_obj_set_ext_click_area(s_country, 6);
    lv_obj_add_event_cb(s_country, country_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_align(s_country, LV_ALIGN_TOP_MID, 0, COUNTRY_Y);
    s_country_flag = lv_image_create(s_country);
    lv_image_set_inner_align(s_country_flag, LV_IMAGE_ALIGN_STRETCH); /* bandera de 26 px a 16 */
    lv_obj_set_clickable(s_country_flag, false);
    s_country_lbl = lv_label_create(s_country);
    lv_label_set_text(s_country_lbl, "");
    lv_obj_set_style_text_font(s_country_lbl, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_country_lbl, COL_NAVY_DARK, 0);
    lv_obj_set_style_max_width(s_country_lbl, 96, 0);
    lv_label_set_long_mode(s_country_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_hidden(s_country, true);

    s_lbl_hold = lv_label_create(s_scr);
    lv_label_set_text(s_lbl_hold, "");
    lv_obj_set_size(s_lbl_hold, 224, HOLD_H);
    lv_obj_set_pos(s_lbl_hold, 8, ICON_Y + 2);
    lv_obj_set_style_text_font(s_lbl_hold, &lv_font_es_14, 0);
    lv_obj_set_style_text_align(s_lbl_hold, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_hor(s_lbl_hold, 6, 0);
    lv_obj_set_style_pad_top(s_lbl_hold, 11, 0);
    lv_obj_set_style_radius(s_lbl_hold, 14, 0);
    lv_obj_set_style_border_color(s_lbl_hold, COL_CORAL, 0);
    lv_obj_set_style_bg_color(s_lbl_hold, COL_CARD, 0);
    lv_label_set_long_mode(s_lbl_hold, LV_LABEL_LONG_MODE_WRAP); /* hasta dos lineas */
    lv_obj_set_hidden(s_lbl_hold, true);
}

static void hold_timer_cb(lv_timer_t *t)
{
    (void)t;
    hh_tick(&s_hh, lv_tick_get());
    hold_refresh(false);
}

/* Botones de arriba (WiFi, ajustes): fondo blanco y marco azul de la marca */
static void top_button_style(lv_obj_t *b)
{
    lv_obj_set_style_bg_color(b, COL_CARD, 0);
    lv_obj_set_style_bg_color(b, COL_BG_PHOTO, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, COL_NAVY, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
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
    lv_obj_set_style_border_width(s_info, 2, 0);
    lv_obj_set_style_radius(s_info, 12, 0);
    lv_obj_set_style_pad_hor(s_info, 10, 0);
    lv_obj_set_style_pad_ver(s_info, 7, 0);
    lv_label_set_long_mode(s_info, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_clickable(s_info, false);
    lv_obj_set_hidden(s_info, true);
}

/* ----------------------------------------------------------------- build */

lv_obj_t *ui_home_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_scr, false);

    /* imagenes a pantalla completa (debajo de todo) */
    s_empty_img = lv_image_create(s_scr);
    lv_image_set_src(s_empty_img, &img_incunest_empty);
    lv_obj_set_pos(s_empty_img, 0, 0);
    lv_obj_set_hidden(s_empty_img, true);

    s_parents_img = lv_image_create(s_scr);
    lv_image_set_src(s_parents_img, &img_baby_parents);
    lv_obj_set_pos(s_parents_img, 0, 0);
    lv_obj_set_hidden(s_parents_img, true);

    /* halo que respira */
    s_halo = lv_obj_create(s_scr);
    lv_obj_set_size(s_halo, 190, 190);
    lv_obj_set_style_radius(s_halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_halo, COL_HALO_CALM, 0);
    lv_obj_set_style_bg_opa(s_halo, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_halo, 0, 0);
    lv_obj_set_pos(s_halo, BABY_CX - 95, BABY_CY - 95);
    lv_obj_set_scrollable(s_halo, false);
    lv_obj_set_clickable(s_halo, false);
    halo_start_anim(2600);

    hold_create_below_baby();

    lv_obj_t *baby = baby_widget_create(s_scr, BABY_X, BABY_Y);
    lv_obj_set_clickable(baby, true);
    lv_obj_add_event_cb(baby, baby_touched, LV_EVENT_ALL, NULL);

    s_icon_thermo = state_icon_create(s_scr, 8, &img_icon_thermo);
    s_icon_photo = state_icon_create(s_scr, 58, &img_icon_lamp);
    lv_obj_t *tap[] = { s_icon_thermo, s_icon_photo };
    lv_event_cb_t tap_cb[] = { thermo_icon_clicked, photo_icon_clicked };
    for (int i = 0; i < 2; i++) { /* tocar: que es este tratamiento */
        lv_obj_set_clickable(tap[i], true);
        lv_obj_add_flag(tap[i], UI_FLAG_CLICK_SOUND); /* suenan como un boton */
        lv_obj_set_ext_click_area(tap[i], 4);
        lv_obj_add_event_cb(tap[i], tap_cb[i], LV_EVENT_CLICKED, NULL);
    }
    s_icon_heart = state_icon_create(s_scr, 108, &img_heart);
    lv_obj_set_clickable(s_icon_heart, true); /* tocar: oir su latido */
    lv_obj_add_flag(s_icon_heart, UI_FLAG_CLICK_SOUND);
    lv_obj_set_ext_click_area(s_icon_heart, 4);
    lv_obj_add_event_cb(s_icon_heart, heart_icon_clicked, LV_EVENT_CLICKED, NULL);

    s_btn_hand = lv_button_create(s_scr);
    lv_obj_set_size(s_btn_hand, 74, ICON_D);
    lv_obj_set_pos(s_btn_hand, 158, ICON_Y);
    lv_obj_set_style_bg_color(s_btn_hand, COL_CORAL, 0);
    lv_obj_set_style_radius(s_btn_hand, 14, 0);
    lv_obj_set_style_shadow_width(s_btn_hand, 0, 0);
    lv_obj_add_event_cb(s_btn_hand, hand_btn_clicked, LV_EVENT_CLICKED, NULL);
    s_lbl_hand = lv_label_create(s_btn_hand);
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    lv_obj_set_style_text_font(s_lbl_hand, &lv_font_es_12, 0);
    lv_obj_set_style_text_align(s_lbl_hand, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_hand, 66);
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

    /* barra de estado */
    s_status = lv_obj_create(s_scr);
    lv_obj_set_size(s_status, 224, 26);
    lv_obj_set_pos(s_status, 8, STATUS_Y);
    lv_obj_set_style_radius(s_status, 10, 0);
    lv_obj_set_style_bg_color(s_status, COL_CARD, 0);
    lv_obj_set_style_border_width(s_status, 2, 0);
    lv_obj_set_style_border_color(s_status, COL_OFFLINE, 0);
    lv_obj_set_style_pad_all(s_status, 0, 0);
    lv_obj_set_scrollable(s_status, false);
    lv_obj_set_clickable(s_status, false);
    s_lbl_status = lv_label_create(s_status);
    lv_label_set_text(s_lbl_status, "");
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_es_14, 0);
    lv_obj_set_style_text_color(s_lbl_status, COL_OFFLINE, 0);
    lv_obj_set_size(s_lbl_status, 216, 18); /* alto fijo: DOTS recorta en vez de saltar de linea */
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_lbl_status);

    /* barra superior */
    /* WiFi y ajustes: botones con marco, para que se vea que se pueden tocar */
    s_wifi_btn = lv_button_create(s_scr);
    lv_obj_set_size(s_wifi_btn, 42, 30);
    lv_obj_align(s_wifi_btn, LV_ALIGN_TOP_LEFT, 4, 5);
    top_button_style(s_wifi_btn);
    lv_obj_set_ext_click_area(s_wifi_btn, 6);
    lv_obj_add_event_cb(s_wifi_btn, wifi_icon_clicked, LV_EVENT_SHORT_CLICKED, NULL); /* cambiar de red */
    s_wifi_img = lv_image_create(s_wifi_btn);
    lv_image_set_src(s_wifi_img, &img_wifi_off);
    lv_obj_center(s_wifi_img);

    s_demo_badge = lv_label_create(s_scr);
    lv_label_set_text(s_demo_badge, "DEMO");
    lv_obj_set_style_text_font(s_demo_badge, &lv_font_es_12, 0);
    lv_obj_set_style_text_color(s_demo_badge, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_demo_badge, COL_CORAL, 0);
    lv_obj_set_style_bg_opa(s_demo_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_demo_badge, 8, 0);
    lv_obj_set_style_pad_hor(s_demo_badge, 4, 0);
    lv_obj_set_style_pad_ver(s_demo_badge, 3, 0);
    lv_obj_set_pos(s_demo_badge, 6, 11);
    lv_obj_set_hidden(s_demo_badge, true);

    lv_obj_t *wm = lv_image_create(s_scr);
    lv_image_set_src(wm, &img_wordmark);
    lv_obj_align(wm, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *gear = lv_button_create(s_scr);
    lv_obj_set_size(gear, 42, 30);
    lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -4, 5);
    top_button_style(gear);
    lv_obj_add_event_cb(gear, gear_clicked, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(gear, gear_long_pressed, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_t *gl = lv_label_create(gear);
    lv_label_set_text(gl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(gl, &lv_font_es_16, 0);
    lv_obj_set_style_text_color(gl, COL_NAVY, 0);
    lv_obj_center(gl);
    return s_scr;
}

/* Cualquier pulsacion en la pantalla: en la despedida del modo acompañar la cierra (pasados unos
 * segundos, por los toques fantasma del tactil). */
void ui_home_screen_pressed(void)
{
    if (!s_farewell || lv_tick_elaps(s_farewell_ms) < FAREWELL_TOUCH_MIN_MS) return;
    s_farewell = false; /* un aviso por despedida */
    app_evt_acc_input_t in = APP_ACC_TOUCH;
    app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
}

void ui_home_retranslate(void)
{
    lv_label_set_text(s_lbl_hand, tr(STR_HAND));
    ui_pages_retranslate();
    hold_refresh(true);
}

/* ------------------------------------------------------------ state -> UI */

/* Fuente de 14 si cabe en una linea de `w`; si no, la de 12 */
static const lv_font_t *fit_font(const char *txt, int32_t w)
{
    lv_point_t sz;
    lv_text_get_size(&sz, txt, &lv_font_es_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x <= w ? &lv_font_es_14 : &lv_font_es_12;
}

static void status_bar_text(const char *txt, lv_color_t col)
{
    if (strcmp(lv_label_get_text(s_lbl_status), txt) != 0) {
        lv_label_set_text(s_lbl_status, txt);
        lv_obj_set_style_text_font(s_lbl_status, fit_font(txt, 216), 0);
    }
    lv_obj_set_style_text_color(s_lbl_status, col, 0);
    lv_obj_set_style_border_color(s_status, col, 0);
}

/* "Lucía · 1250 g · 3 días" (solo los campos compartidos) */
static void status_bar_baby(const twin_incubator_t *inc)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%s", inc->name);
    if (inc->weight_g > 0 && n < (int)sizeof(buf))
        n += snprintf(buf + n, sizeof(buf) - n, " · %u g", inc->weight_g);
    if (inc->age_d >= 0 && n < (int)sizeof(buf))
        snprintf(buf + n, sizeof(buf) - n, " · %d %s", inc->age_d,
                 tr(inc->age_d == 1 ? STR_DAY : STR_DAYS));
    status_bar_text(buf, COL_OK);
}

static void status_bar_apply(const twin_snapshot_t *s)
{
    twin_status_t st = twin_status_of(s);
    /* en la pagina del mapa, la hora del pais (salvo avisos que no pueden esperar) */
    char clock[64];
    bool urgent = st == TWIN_ST_ALARM || st == TWIN_ST_NO_WIFI || st == TWIN_ST_NO_CREDS ||
                  st == TWIN_ST_BROKER_LOST;
    if (!urgent && ui_pages_status_text(clock, sizeof(clock))) {
        status_bar_text(clock, COL_NAVY_DARK);
        return;
    }
    switch (st) {
    case TWIN_ST_HOME:        status_bar_text(tr(STR_ST_HOME), COL_OK); break;
    case TWIN_ST_PARENTS:     status_bar_text(tr(STR_ST_PARENTS), COL_OK); break;
    case TWIN_ST_NO_WIFI:     status_bar_text(tr(STR_ST_NO_WIFI), COL_RED); break;
    case TWIN_ST_NO_CREDS:    status_bar_text(tr(STR_ST_NO_CREDS), COL_RED); break;
    case TWIN_ST_CONNECTING:  status_bar_text(tr(STR_ST_CONNECTING), COL_WARM); break;
    case TWIN_ST_BROKER_LOST: status_bar_text(tr(STR_ST_BROKER_LOST), COL_RED); break;
    case TWIN_ST_UNPAIRED:    status_bar_text(tr(STR_ST_UNPAIRED), COL_OFFLINE); break;
    case TWIN_ST_WAITING:     status_bar_text(tr(STR_ST_WAITING), COL_OFFLINE); break;
    case TWIN_ST_INC_OFF:     status_bar_text(tr(STR_ST_INC_OFF), COL_OFFLINE); break;
    case TWIN_ST_NO_BABY:     status_bar_text(tr(STR_ST_NO_BABY), COL_PHOTO); break;
    case TWIN_ST_ALARM:       status_bar_text(tr(STR_ST_ALARM), COL_RED); break;
    case TWIN_ST_NAME:        status_bar_baby(&s->inc); break;
    case TWIN_ST_ACC_SEARCH:  status_bar_text(tr(STR_ACC_SEARCH), COL_OFFLINE); break;
    case TWIN_ST_ACC_WAITING: status_bar_text(tr(STR_ACC_WAITING), COL_CORAL); break;
    case TWIN_ST_ACC_REMIND:  status_bar_text(tr(STR_ACC_REMIND), COL_CORAL); break;
    case TWIN_ST_ACC_ENDED:   status_bar_text(tr(STR_ACC_ENDED), COL_OFFLINE); break;
    case TWIN_ST_ACC_FAREWELL: status_bar_text(tr(STR_ACC_FAREWELL), COL_OK); break;
    case TWIN_ST_AWAKE:       status_bar_text(tr(STR_ST_AWAKE), COL_OK); break;
    default:                  status_bar_text(tr(STR_ST_SLEEP), COL_OK); break;
    }
}

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
    lv_obj_set_hidden(s_btn_hand, !shown);
}

static void home_set_view(home_view_t view)
{
    lv_obj_set_hidden(baby_widget_obj(), view != VIEW_BABY);
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
    if (lv_image_get_src(s_parents_img) != src) lv_image_set_src(s_parents_img, src);
}

static void wifi_icon_apply(const twin_snapshot_t *s)
{
    if (s->demo) {
        lv_obj_set_hidden(s_wifi_btn, true);
        lv_obj_set_hidden(s_demo_badge, false);
        return;
    }
    lv_obj_set_hidden(s_demo_badge, true);
    lv_obj_set_hidden(s_wifi_btn, false);
    if (!s->link.wifi) {
        lv_image_set_src(s_wifi_img, &img_wifi_off);
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
        return;
    }
    static const lv_image_dsc_t *const LVL[] = { &img_wifi_0, &img_wifi_1, &img_wifi_2, &img_wifi_3 };
    lv_image_set_src(s_wifi_img, LVL[twin_rssi_level(s->link.rssi)]);
    if (s->link.broker_connected) {
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_TRANSP, 0);
    } else { /* ambar: hay WiFi pero no sesion con el broker */
        lv_obj_set_style_image_recolor(s_wifi_img, COL_WARM, 0);
        lv_obj_set_style_image_recolor_opa(s_wifi_img, LV_OPA_COVER, 0);
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
    lv_text_get_size(&sz, t, &lv_font_es_14, 0, 0, 224 - 12, LV_TEXT_FLAG_NONE);
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

/* Nombre y pais: con el bebe a la vista, salvo mientras se agradece (imagen final) */
static void overlay_refresh(void)
{
    if (!s_have_last) return;
    const twin_snapshot_t *s = &s_last;
    bool thanks = hh_state(&s_hh) == HH_THANKS;
    bool show_name = s->show_baby && s->inc.name[0] != '\0' && !thanks;
    if (show_name) label_set(s_name, s->inc.name);
    lv_obj_set_hidden(s_name, !show_name);

    /* encima del cuerpo: fuera mientras dura el gesto, que el dedo va justo ahi */
    bool show_country = (s->show_baby || s->parents_view) && s->inc.country[0] != '\0' &&
                        hh_state(&s_hh) == HH_IDLE;
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
    lv_obj_set_style_bg_color(s_scr, s_halo_mode == HALO_PHOTO ? COL_BG_PHOTO : COL_BG, 0);
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

    if (st == HH_DONE && s_hh_shown == HH_HOLDING) {
        ui_pages_note_hand(); /* +1 en los logros */
        if (s_have_last && !s_last.demo) { /* modo acompañar: hoy cuenta (si esta apagado, nada) */
            app_evt_acc_input_t in = APP_ACC_GESTURE;
            app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
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
        overlay_refresh();
        if (idle) {
            s_ring_lap = -1;
            s_super_sent = false;
        }

        bool boxed = st == HH_READY || st == HH_THANKS;
        lv_obj_set_style_bg_opa(s_lbl_hold, boxed ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_lbl_hold, boxed ? 2 : 0, 0);
        lv_obj_set_style_text_color(s_lbl_hold,
                                    (st == HH_READY || st == HH_GRACE) ? COL_CORAL : COL_NAVY_DARK, 0);
        if (baby_view && (st == HH_DONE || st == HH_THANKS)) {
            lv_obj_set_style_bg_color(s_scr, COL_BG_THANKS, 0);
        } else {
            scr_bg_restore();
        }
        if (idle) { /* fin del gesto: vuelve el estado real */
            baby_set_awake(s_awake_real);
            if (s_have_last) status_bar_apply(&s_last);
        } else { /* listo y gracia: te espera con los ojos abiertos; con tu dedo, los cierra */
            baby_set_awake(st == HH_READY || st == HH_GRACE);
        }
    }
    if (idle || !baby_view) return;

    uint16_t p = hh_progress_permille(&s_hh, now);
    ring_apply(hh_held_ms(&s_hh, now), st == HH_GRACE);
    char buf[48];
    switch (st) {
    case HH_READY:
        hold_text(tr(STR_HH_READY));
        status_bar_text(tr(STR_HH_ST_READY), COL_CORAL);
        break;
    case HH_HOLDING:
        hold_text(tr(p < 500 ? STR_HH_HOLD1 : STR_HH_HOLD2));
        status_bar_text(tr(STR_HH_ST_HOLD), COL_CORAL);
        break;
    case HH_GRACE:
        hold_text(tr(STR_HH_GRACE));
        snprintf(buf, sizeof(buf), tr(STR_HH_ST_GRACE), (unsigned)hh_grace_left_s(&s_hh, now));
        status_bar_text(buf, COL_CORAL);
        break;
    default: /* completado, con o sin dedo */
        hold_text(tr(STR_HH_THANKS));
        if (st == HH_DONE && s_last.acc != TWIN_ACC_OFF && !s_last.demo) {
            /* modo acompañar: sigue con el dedo hasta los 3 min y es un super corazon */
            uint32_t held_s = hh_held_ms(&s_hh, now) / 1000;
            if (held_s >= ACC_SUPER_S) {
                if (!s_super_sent) {
                    s_super_sent = true;
                    app_evt_acc_input_t in = APP_ACC_SUPER;
                    app_events_post(TWIN_EVT_ACC_INPUT, &in, sizeof(in));
                }
                status_bar_text(tr(STR_HH_ST_SUPER), COL_SUPER);
            } else {
                unsigned left = ACC_SUPER_S - held_s;
                snprintf(buf, sizeof(buf), tr(STR_HH_ST_SUPER_IN), left / 60, left % 60);
                status_bar_text(buf, COL_OK);
            }
        } else {
            /* con el dedo puesto sigue sumando; ya soltado, el gesto ha terminado */
            status_bar_text(tr(st == HH_DONE ? STR_HH_ST_DONE : STR_HH_ST_AGAIN), COL_OK);
        }
        break;
    }
}

void ui_home_apply(const twin_snapshot_t *s)
{
    const twin_incubator_t *inc = &s->inc;
    s_awake_real = inc->awake;
    s_last = *s;
    s_have_last = true;
    bool farewell = s->acc == TWIN_ACC_FAREWELL;
    if (farewell && !s_farewell) s_farewell_ms = lv_tick_get();
    s_farewell = farewell;
    ui_pages_apply(s);
    hh_set_available(&s_hh, s->show_baby && ui_pages_current() == 0,
                     !s->demo && s->link.broker_connected && s->link.paired, lv_tick_get());
    if (s_hb_timer && !(s->show_baby && inc->bpm > 0)) heart_sound_set(false);
    bool hh_idle = hh_state(&s_hh) == HH_IDLE;

    wifi_icon_apply(s);
    home_set_view(s->parents_view ? VIEW_PARENTS : s->show_baby ? VIEW_BABY : VIEW_EMPTY);
    if (hh_idle) status_bar_apply(s);

    overlay_refresh();
    /* El sexo lo decide ThingsBoard (Contrato v0.2 §3.4.1); desconocido = nino, el dibujo
     * de siempre. Afecta al bebe animado y a la escena "con sus papas" / "en casa". */
    bool girl = inc->sex == TWIN_SEX_FEMALE;
    parents_img_apply(inc->skin, girl);

    lv_color_t tcol = inc->thermo == TWIN_THERMO_HEATING ? COL_WARM
                    : inc->thermo == TWIN_THERMO_ALARM   ? COL_RED : COL_OK;
    treat_icon_set(s_icon_thermo, s->show_baby && inc->thermo != TWIN_THERMO_OFF, tcol);
    treat_icon_set(s_icon_photo, s->show_baby && inc->photo, COL_PHOTO);
    state_icon_set(s_icon_heart, COL_CORAL);
    row_apply(s_row_shown); /* el tratamiento puede haber cambiado */

    baby_set_skin_tone(inc->skin);
    baby_set_girl(girl);
    if (hh_idle) baby_set_awake(inc->awake); /* durante el gesto los ojos los pone el gesto */
    baby_set_heart_rate(s->show_baby ? inc->bpm : 0);

    bool calling = s->acc == TWIN_ACC_WAITING || (s->acc != TWIN_ACC_OFF && s->acc_remind);
    if (!s->show_baby)                          halo_set_mode(HALO_OFF);
    else if (inc->thermo == TWIN_THERMO_ALARM)   halo_set_mode(HALO_ALARM);
    else if (calling)                            halo_set_mode(HALO_CALL);
    else if (inc->photo)                         halo_set_mode(HALO_PHOTO);
    else if (inc->thermo == TWIN_THERMO_HEATING) halo_set_mode(HALO_WARM);
    else                                         halo_set_mode(HALO_CALM);

    /* el modo del halo puede haber repintado el fondo: el gesto manda sobre el */
    hold_refresh(false);
    if (!hh_idle && s->show_baby && (hh_state(&s_hh) == HH_DONE || hh_state(&s_hh) == HH_THANKS)) {
        lv_obj_set_style_bg_color(s_scr, COL_BG_THANKS, 0);
    }
}
