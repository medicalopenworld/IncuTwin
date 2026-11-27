/* Aviso "Ha llegado un bebe" y teclado para ponerle nombre (spec nombre-bebe-panel).
 *
 * El nombre es solo del usuario de esta IncuTwin (baby_name, NVS). El teclado es una
 * lv_buttonmatrix propia: el lv_keyboard de LVGL usa simbolos (OK, borrar) que no estan en
 * nuestras fuentes, y aqui hace falta la Ñ.
 *
 * El aviso y el teclado trabajan sobre la estancia que habia al abrirse (s_key): si mientras
 * tanto cambia el estado (alarma, demo, otro bebe), ui_home los cierra con ui_name_close() sin
 * contar como preguntado. */
#include "ui_name.h"

#include <stddef.h>
#include <stdio.h>

#include "assets.h"
#include "baby_name.h"
#include "name_logic.h"
#include "ui_internal.h"
#include "ui_toy.h"

#define KB_IDLE_MS 60000 /* teclado sin tocar: se cierra (toques fantasma del tactil) */

static lv_obj_t *s_mb;       /* aviso */
static bool s_mb_yes;        /* el aviso se cierra porque se ha dicho SI */
static bool s_mb_silent;     /* ... o porque lo cierra ui_name_close(): no cuenta como preguntado */
static lv_obj_t *s_kb;       /* capa del teclado */
static lv_obj_t *s_ta;
static lv_timer_t *s_kb_timer;
static char s_key[BN_KEY_LEN]; /* estancia del aviso o del teclado abiertos */
static void (*s_done_cb)(void);
/* modo texto libre (nombre de la IncuTwin): no toca baby_name; NULL = nombre del bebe */
static void (*s_text_submit)(const char *text);

/* Teclado espanol de ordenador, sin vocales con tilde (Pablo, 24-nov-2026): Q-P, A-Ñ, Z-M y
 * borrar, espacio y LISTO. Ids de la matriz: 0..9 Q-P, 10..19 A-Ñ, 20..26 Z-M, 27 borrar,
 * 28 espacio, 29 LISTO. */
enum { KEY_BACKSPACE = 27, KEY_SPACE = 28, KEY_DONE = 29 };
static const char *s_map[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ", "\n",
    "Z", "X", "C", "V", "B", "N", "M", "<", "\n",
    " ", "LISTO", "",
};
#define MAP_DONE_IDX 32 /* posicion de "LISTO" en s_map (cuenta los "\n") */
_Static_assert(sizeof(s_map) / sizeof(s_map[0]) == MAP_DONE_IDX + 2, "s_map y MAP_DONE_IDX");

void ui_name_on_done(void (*cb)(void)) { s_done_cb = cb; }

bool ui_name_open(void) { return s_mb || s_kb; }

const char *ui_name_open_key(void) { return s_key; }

static void done(void)
{
    if (!ui_name_open() && s_done_cb) s_done_cb();
}

/* ---------------------------------------------------------------- teclado */

static void kb_close(void)
{
    if (!s_kb) return;
    if (s_kb_timer) {
        lv_timer_delete(s_kb_timer);
        s_kb_timer = NULL;
    }
    lv_obj_delete_async(s_kb);
    s_kb = NULL;
    s_ta = NULL;
    s_text_submit = NULL;
    done();
}

static void kb_submit(void)
{
    if (s_text_submit) {
        s_text_submit(lv_textarea_get_text(s_ta));
        kb_close();
        return;
    }
    baby_name_set(s_key, lv_textarea_get_text(s_ta)); /* vacio: no cambia nada */
    baby_name_dismiss(s_key);                         /* en cualquier caso, ya se ha preguntado */
    kb_close();
}

static void kb_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (!s_text_submit) baby_name_dismiss(s_key);
    kb_close();
}

static void kb_idle_cb(lv_timer_t *t)
{
    (void)t;
    s_kb_timer = NULL; /* el timer de una vez se borra solo */
    kb_close();        /* sin contar como preguntado: el aviso puede volver a salir */
}

static void kb_key_cb(lv_event_t *e)
{
    lv_obj_t *m = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(m);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE || !s_ta) return;
    if (s_kb_timer) lv_timer_reset(s_kb_timer);
    if (id == KEY_DONE) { kb_submit(); return; }
    if (id == KEY_BACKSPACE) { lv_textarea_delete_char(s_ta); return; }
    if (id == KEY_SPACE) {
        const char *t = lv_textarea_get_text(s_ta);
        if (t[0] && !bn_upper_next(t)) lv_textarea_add_char(s_ta, ' '); /* sin espacios dobles */
        return;
    }
    char out[5];
    bn_key_case(lv_buttonmatrix_get_button_text(m, id), bn_upper_next(lv_textarea_get_text(s_ta)), out);
    lv_textarea_add_text(s_ta, out);
}

static void kb_build(const char *title_txt, const char *initial, uint32_t max_chars);

void ui_name_edit(void)
{
    if (s_kb) return;
    if (!s_mb) snprintf(s_key, sizeof(s_key), "%s", baby_name_key()); /* desde el aviso ya esta */
    s_text_submit = NULL;
    kb_build(tr(STR_NAME_PLACEHOLDER), baby_name_local(), BN_NAME_MAX_CHARS);
}

void ui_name_edit_text(const char *title, const char *initial, uint32_t max_chars, void (*submit)(const char *text))
{
    if (ui_name_open() || !submit) return;
    s_key[0] = 0;
    kb_build(title, initial, max_chars);
    s_text_submit = submit;
}

static void kb_build(const char *title_txt, const char *initial, uint32_t max_chars)
{
    s_map[MAP_DONE_IDX] = tr(STR_NAME_DONE);

    s_kb = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_kb);
    lv_obj_set_size(s_kb, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_kb, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_kb, LV_OPA_COVER, 0);
    lv_obj_set_clickable(s_kb, true); /* que no pase nada a la pantalla de debajo */
    lv_obj_set_scrollable(s_kb, false);

    lv_obj_t *title = lv_label_create(s_kb);
    lv_label_set_text(title, title_txt);
    lv_obj_set_style_text_font(title, &lv_font_title_16, 0);
    lv_obj_set_style_text_color(title, COL_INK, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 14);

    lv_obj_t *cancel = lv_button_create(s_kb);
    lv_obj_t *cl = lv_label_create(cancel);
    lv_label_set_text(cl, tr(STR_OB_CANCEL));
    toy_button(cancel, false); /* crema con borde tinta: la accion secundaria */
    lv_obj_align(cancel, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(cancel, kb_cancel_cb, LV_EVENT_CLICKED, NULL);

    s_ta = lv_textarea_create(s_kb);
    lv_textarea_set_one_line(s_ta, true);
    lv_textarea_set_max_length(s_ta, max_chars);
    lv_textarea_set_text(s_ta, initial ? initial : "");
    lv_obj_set_width(s_ta, LV_PCT(92));
    lv_obj_set_style_text_font(s_ta, &lv_font_title_20, 0);
    lv_obj_set_style_text_color(s_ta, COL_INK, 0);
    toy_box(s_ta, TOY_R_CARD, TOY_SHADOW_S); /* campo de juguete, como los de la app */
    lv_obj_set_style_border_color(s_ta, COL_INK, LV_STATE_FOCUSED);
    lv_obj_add_state(s_ta, LV_STATE_FOCUSED); /* cursor visible */
    lv_obj_align(s_ta, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *m = lv_buttonmatrix_create(s_kb);
    lv_buttonmatrix_set_map(m, s_map);
    lv_buttonmatrix_set_button_width(m, KEY_BACKSPACE, 3);
    lv_buttonmatrix_set_button_width(m, KEY_SPACE, 3);
    lv_buttonmatrix_set_button_width(m, KEY_DONE, 2);
    lv_buttonmatrix_set_button_ctrl(m, KEY_DONE, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(m, LV_PCT(100), 200);
    lv_obj_align(m, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(m, 3, 0);
    lv_obj_set_style_pad_gap(m, 3, 0);
    lv_obj_set_style_text_font(m, &lv_font_title_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    /* teclas de juguete: crema con borde tinta; Hecho, coral */
    lv_obj_set_style_bg_color(m, COL_CARD, LV_PART_ITEMS);
    lv_obj_set_style_border_color(m, COL_INK, LV_PART_ITEMS);
    lv_obj_set_style_border_width(m, TOY_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_radius(m, 8, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(m, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(m, COL_INK, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(m, COL_TAB_ON, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(m, COL_CORAL, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(m, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_flag(m, UI_FLAG_CLICK_SOUND);
    lv_obj_add_event_cb(m, kb_key_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_kb_timer = lv_timer_create(kb_idle_cb, KB_IDLE_MS, NULL);
    lv_timer_set_repeat_count(s_kb_timer, 1);
}

/* ------------------------------------------------------------------ aviso */

static void mb_deleted_cb(lv_event_t *e)
{
    (void)e;
    if (!s_mb_yes && !s_mb_silent) baby_name_dismiss(s_key); /* AHORA NO o la X */
    s_mb = NULL;
    done();
}

static void mb_yes_cb(lv_event_t *e)
{
    (void)e;
    s_mb_yes = true;
    ui_name_edit(); /* antes de cerrar: ui_name_open() sigue siendo true */
    lv_msgbox_close_async(s_mb);
}

static void mb_later_cb(lv_event_t *e)
{
    (void)e;
    lv_msgbox_close_async(s_mb);
}

void ui_name_ask(void)
{
    if (ui_name_open()) return;
    snprintf(s_key, sizeof(s_key), "%s", baby_name_key());
    s_mb_yes = false;
    s_mb_silent = false;
    s_mb = ui_msgbox(tr(STR_NAME_ARRIVED), tr(STR_NAME_ASK), tr(STR_NAME_YES), COL_CORAL, mb_yes_cb, NULL);
    lv_obj_t *later = lv_msgbox_add_footer_button(s_mb, tr(STR_NAME_LATER));
    lv_obj_add_flag(later, UI_FLAG_CLICK_SOUND);
    toy_button(later, false);
    lv_obj_add_event_cb(later, mb_later_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_mb, mb_deleted_cb, LV_EVENT_DELETE, NULL);
}

void ui_name_close(void)
{
    if (s_text_submit) return; /* el nombre de la IncuTwin no depende del estado del bebe */
    if (s_mb && !s_mb_yes) {
        s_mb_silent = true;
        lv_msgbox_close_async(s_mb);
    }
    kb_close();
}
