/* Bebe sin WiFi, busqueda del asistente y celebracion al conectar (ver ui_nowifi.h). Los
 * fotogramas, posiciones, tiempos y particulas salen de tools/gen_nowifi.py (assets_nowifi.h). */
#include "ui_nowifi.h"

#include <stdint.h>

#include "assets_nowifi.h"
#include "fonts.h"
#include "ui_i18n.h"
#include "ui_theme.h"

#define BREATH_PX   3
#define BREATH_MS   2600 /* por sentido, como el bebe de Inicio */
#define PART_TICK   33
#define FADE_OUT_MS 400

typedef struct { int8_t body, sym; uint16_t ms; } step_t;
typedef struct { int8_t body, sym; uint16_t ms; int8_t dy; } cstep_t;
typedef struct { uint8_t kind; int16_t vx, vy; uint16_t delay; } part_t;
typedef struct { int16_t x, y; } pos_t;

static const step_t SEQ[NOWIFI_SEQ_LEN] = NOWIFI_SEQ;
static const step_t SEARCH[NOWIFI_SEARCH_LEN] = NOWIFI_SEARCH;
static const cstep_t CELEB[NOWIFI_CELEB_LEN] = NOWIFI_CELEB;
static const part_t PARTS[NOWIFI_N_PARTS] = NOWIFI_PARTS;
static const uint32_t CONFETTI[] = NOWIFI_CONFETTI_COLS;
static const pos_t BOY_POS[NOWIFI_BODIES] = NOWIFI_BOY_BODY_POS;
static const pos_t GIRL_POS[NOWIFI_BODIES] = NOWIFI_GIRL_BODY_POS;
static const pos_t SYM_POS[NOWIFI_SYMS] = NOWIFI_SYM_POS;
static const lv_image_dsc_t *const BOY[NOWIFI_BODIES] = {
    &img_nowifi_boy_0, &img_nowifi_boy_1, &img_nowifi_boy_2, &img_nowifi_boy_3, &img_nowifi_boy_4,
};
static const lv_image_dsc_t *const GIRL[NOWIFI_BODIES] = {
    &img_nowifi_girl_0, &img_nowifi_girl_1, &img_nowifi_girl_2, &img_nowifi_girl_3, &img_nowifi_girl_4,
};
static const lv_image_dsc_t *const SYM[NOWIFI_SYMS] = {
    &img_nowifi_sym0, &img_nowifi_sym1, &img_nowifi_sym2, &img_nowifi_sym3, &img_nowifi_sym4,
    &img_nowifi_sym5, &img_nowifi_sym6, &img_nowifi_sym7, &img_nowifi_sym8, &img_nowifi_sym9,
};

typedef enum { M_OFF = 0, M_LOOP, M_SEARCH, M_CELEB, M_FADE } nw_mode_t;

struct ui_nowifi {
    lv_obj_t *parent, *box, *body, *sym, *text;
    lv_obj_t *part[NOWIFI_N_PARTS];
    lv_timer_t *step_timer, *part_timer;
    nw_mode_t mode;
    bool girl;
    uint16_t idx;
    uint8_t hops;
    uint32_t burst_ms;
    int32_t x, y;
    void (*done)(void);
};

static void frame_set(ui_nowifi_t *nw, int8_t body, int8_t sym, int32_t dy)
{
    const pos_t *pos = nw->girl ? GIRL_POS : BOY_POS;
    const lv_image_dsc_t *src = (nw->girl ? GIRL : BOY)[body];
    if (lv_image_get_src(nw->body) != src) lv_image_set_src(nw->body, src);
    lv_obj_set_pos(nw->body, pos[body].x, pos[body].y + dy);
    lv_obj_set_hidden(nw->sym, sym < 0);
    if (sym >= 0) {
        if (lv_image_get_src(nw->sym) != SYM[sym]) lv_image_set_src(nw->sym, SYM[sym]);
        lv_obj_set_pos(nw->sym, SYM_POS[sym].x, SYM_POS[sym].y + dy);
    }
}

static void text_set(ui_nowifi_t *nw)
{
    bool back = nw->mode == M_CELEB || nw->mode == M_FADE;
    ui_str_t id = back ? STR_NOWIFI_BACK : nw->mode == M_SEARCH ? STR_OB_CONNECTING_WIFI : STR_NOWIFI_MSG;
    lv_label_set_text(nw->text, tr(id));
    lv_obj_set_style_text_color(nw->text, back ? COL_OK : COL_NAVY_DARK, 0);
}

/* ------------------------------------------------------------ respiracion */

static void breath_cb(void *var, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)var, -v, 0); }

static void breath_set(ui_nowifi_t *nw, bool on)
{
    lv_anim_delete(nw->box, breath_cb);
    lv_obj_set_style_translate_y(nw->box, 0, 0);
    if (!on) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, nw->box);
    lv_anim_set_exec_cb(&a, breath_cb);
    lv_anim_set_values(&a, 0, BREATH_PX);
    lv_anim_set_duration(&a, BREATH_MS);
    lv_anim_set_reverse_duration(&a, BREATH_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* ------------------------------------------------------------- particulas */

static void parts_clear(ui_nowifi_t *nw)
{
    if (nw->part_timer) {
        lv_timer_delete(nw->part_timer);
        nw->part_timer = NULL;
    }
    for (int i = 0; i < NOWIFI_N_PARTS; i++) {
        if (nw->part[i]) lv_obj_delete(nw->part[i]);
        nw->part[i] = NULL;
    }
}

static void parts_tick(lv_timer_t *t)
{
    ui_nowifi_t *nw = lv_timer_get_user_data(t);
    uint32_t el = lv_tick_elaps(nw->burst_ms);
    int32_t tx = nw->x + (nw->girl ? NOWIFI_GIRL_TIP_X : NOWIFI_BOY_TIP_X);
    int32_t ty = nw->y + (nw->girl ? NOWIFI_GIRL_TIP_Y : NOWIFI_BOY_TIP_Y);
    bool alive = false;
    for (int i = 0; i < NOWIFI_N_PARTS; i++) {
        lv_obj_t *o = nw->part[i];
        int32_t pt = (int32_t)el - PARTS[i].delay;
        bool on = pt >= 0 && pt <= NOWIFI_LIFE_MS;
        lv_obj_set_hidden(o, !on);
        if (pt <= NOWIFI_LIFE_MS) alive = true;
        if (!on) continue;
        int32_t x = tx + PARTS[i].vx * pt / 1000;
        int32_t y = ty + PARTS[i].vy * pt / 1000 + (int32_t)((int64_t)NOWIFI_G * pt * pt / 2000000);
        lv_obj_set_pos(o, x - lv_obj_get_width(o) / 2, y - lv_obj_get_height(o) / 2);
        int32_t left = NOWIFI_LIFE_MS - pt;
        lv_obj_set_style_opa(o, left >= NOWIFI_FADE_MS ? LV_OPA_COVER : (lv_opa_t)(255 * left / NOWIFI_FADE_MS), 0);
    }
    if (!alive) parts_clear(nw);
}

static void parts_burst(ui_nowifi_t *nw)
{
    parts_clear(nw);
    for (int i = 0; i < NOWIFI_N_PARTS; i++) {
        lv_obj_t *o;
        uint8_t k = PARTS[i].kind;
        if (k == NOWIFI_PART_HEART || k == NOWIFI_PART_STAR) {
            o = lv_image_create(nw->parent);
            lv_image_set_src(o, k == NOWIFI_PART_HEART ? &img_nowifi_heart : &img_nowifi_star);
        } else {
            o = lv_obj_create(nw->parent);
            lv_obj_remove_style_all(o);
            lv_obj_set_size(o, NOWIFI_CONFETTI_SIZE, NOWIFI_CONFETTI_SIZE);
            lv_obj_set_style_bg_color(o, lv_color_hex(CONFETTI[(k - 2) % (sizeof(CONFETTI) / sizeof(CONFETTI[0]))]), 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        }
        lv_obj_set_clickable(o, false);
        lv_obj_set_scrollable(o, false);
        lv_obj_set_hidden(o, true);
        nw->part[i] = o;
    }
    nw->burst_ms = lv_tick_get();
    nw->part_timer = lv_timer_create(parts_tick, PART_TICK, nw);
    parts_tick(nw->part_timer);
}

/* -------------------------------------------------------------- secuencia */

static void stop_all(ui_nowifi_t *nw)
{
    if (nw->step_timer) {
        lv_timer_delete(nw->step_timer);
        nw->step_timer = NULL;
    }
    lv_anim_delete(nw->box, NULL);
    breath_set(nw, false);
    lv_obj_set_style_opa(nw->box, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(nw->text, LV_OPA_COVER, 0);
    parts_clear(nw);
}

static void finish(ui_nowifi_t *nw)
{
    stop_all(nw);
    lv_obj_set_hidden(nw->box, true);
    lv_obj_set_hidden(nw->text, true);
    nw->mode = M_OFF;
    void (*done)(void) = nw->done;
    nw->done = NULL;
    if (done) done(); /* lo ultimo: puede borrar la pantalla y con ella esta instancia */
}

static void step_cb(lv_timer_t *t);

static void schedule(ui_nowifi_t *nw, uint32_t ms)
{
    if (!nw->step_timer) nw->step_timer = lv_timer_create(step_cb, ms, nw);
    else lv_timer_set_period(nw->step_timer, ms);
    lv_timer_reset(nw->step_timer);
}

static void plain_show(ui_nowifi_t *nw)
{
    const step_t *st = nw->mode == M_SEARCH ? &SEARCH[nw->idx] : &SEQ[nw->idx];
    frame_set(nw, st->body, st->sym, 0);
    schedule(nw, st->ms);
}

static void fade_cb(void *var, int32_t v)
{
    ui_nowifi_t *nw = lv_obj_get_user_data((lv_obj_t *)var);
    lv_obj_set_style_opa(nw->box, (lv_opa_t)v, 0);
    lv_obj_set_style_opa(nw->text, (lv_opa_t)v, 0); /* el texto se va con el bebe */
}

static void fade_done(lv_anim_t *a) { finish(lv_obj_get_user_data((lv_obj_t *)a->var)); }

static void celeb_show(ui_nowifi_t *nw)
{
    const cstep_t *st = &CELEB[nw->idx];
    /* cada vez que entra el cuerpo 3 (antena fuera) empieza un salto */
    if (st->body == 3 && (nw->idx == 0 || CELEB[nw->idx - 1].body != 3)) {
        if (++nw->hops == NOWIFI_CELEB_BURST) parts_burst(nw);
    }
    frame_set(nw, st->body, st->sym, st->dy);
    schedule(nw, st->ms);
}

static void fade_start(ui_nowifi_t *nw)
{
    if (nw->step_timer) {
        lv_timer_delete(nw->step_timer);
        nw->step_timer = NULL;
    }
    nw->mode = M_FADE;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, nw->box);
    lv_anim_set_exec_cb(&a, fade_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, FADE_OUT_MS);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
}

static void step_cb(lv_timer_t *t)
{
    ui_nowifi_t *nw = lv_timer_get_user_data(t);
    switch (nw->mode) {
    case M_LOOP:
        nw->idx = (uint16_t)((nw->idx + 1) % NOWIFI_SEQ_LEN);
        plain_show(nw);
        break;
    case M_SEARCH:
        nw->idx = (uint16_t)(nw->idx + 1 < NOWIFI_SEARCH_LEN ? nw->idx + 1 : NOWIFI_SEARCH_LOOP);
        plain_show(nw);
        break;
    case M_CELEB:
        if (++nw->idx >= NOWIFI_CELEB_LEN) fade_start(nw);
        else celeb_show(nw);
        break;
    default:
        break;
    }
}

/* Se borra el lienzo (con su pantalla): fuera temporizadores y memoria. Las particulas y el
 * texto son hijos de `parent` y se borran con el. */
static void box_deleted(lv_event_t *e)
{
    ui_nowifi_t *nw = lv_event_get_user_data(e);
    if (nw->step_timer) lv_timer_delete(nw->step_timer);
    if (nw->part_timer) lv_timer_delete(nw->part_timer);
    lv_free(nw);
}

static void start(ui_nowifi_t *nw, nw_mode_t mode, bool girl)
{
    stop_all(nw);
    nw->done = NULL;
    nw->girl = girl;
    nw->mode = mode;
    nw->idx = 0;
    lv_obj_set_hidden(nw->box, false);
    lv_obj_set_hidden(nw->text, false);
    text_set(nw);
    breath_set(nw, true);
    plain_show(nw);
}

/* ---------------------------------------------------------------- publico */

ui_nowifi_t *ui_nowifi_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t text_y)
{
    ui_nowifi_t *nw = lv_malloc_zeroed(sizeof(*nw));
    if (!nw) return NULL;
    nw->parent = parent;
    nw->x = x;
    nw->y = y;
    nw->box = lv_obj_create(parent);
    lv_obj_remove_style_all(nw->box);
    lv_obj_set_size(nw->box, NOWIFI_W, NOWIFI_H + 16); /* margen abajo para saltar sin recortar */
    lv_obj_set_pos(nw->box, x, y);
    lv_obj_set_clickable(nw->box, false);
    lv_obj_set_scrollable(nw->box, false);
    lv_obj_set_user_data(nw->box, nw);
    lv_obj_add_event_cb(nw->box, box_deleted, LV_EVENT_DELETE, nw);
    nw->body = lv_image_create(nw->box);
    nw->sym = lv_image_create(nw->box);
    lv_obj_set_hidden(nw->box, true);

    nw->text = lv_label_create(parent);
    lv_obj_set_width(nw->text, 232);
    lv_obj_set_pos(nw->text, 4, text_y);
    lv_obj_set_style_text_font(nw->text, &lv_font_es_14, 0);
    lv_obj_set_style_text_align(nw->text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_clickable(nw->text, false);
    lv_obj_set_hidden(nw->text, true);
    return nw;
}

void ui_nowifi_retranslate(ui_nowifi_t *nw)
{
    if (nw && nw->mode != M_OFF) text_set(nw);
}

void ui_nowifi_show(ui_nowifi_t *nw, bool girl)
{
    if (!nw) return;
    if (nw->mode == M_LOOP) {
        if (girl != nw->girl) { /* cambia el sexo sin reiniciar el bucle */
            nw->girl = girl;
            plain_show(nw);
        }
        return;
    }
    start(nw, M_LOOP, girl); /* tambien a mitad de una celebracion: se ha vuelto a ir el WiFi */
}

void ui_nowifi_search(ui_nowifi_t *nw, bool girl)
{
    if (nw && nw->mode != M_SEARCH) start(nw, M_SEARCH, girl);
}

void ui_nowifi_hide(ui_nowifi_t *nw)
{
    if (!nw || nw->mode == M_OFF) return;
    nw->done = NULL;
    finish(nw);
}

void ui_nowifi_celebrate(ui_nowifi_t *nw, bool girl, void (*done)(void))
{
    if (!nw) return;
    stop_all(nw);
    nw->girl = girl;
    nw->done = done;
    nw->mode = M_CELEB;
    nw->idx = 0;
    nw->hops = 0;
    lv_obj_set_hidden(nw->box, false);
    lv_obj_set_hidden(nw->text, false);
    text_set(nw);
    celeb_show(nw);
}

void ui_nowifi_skip(ui_nowifi_t *nw)
{
    if (nw && nw->mode == M_CELEB) fade_start(nw);
}

bool ui_nowifi_active(const ui_nowifi_t *nw) { return nw && nw->mode != M_OFF; }
bool ui_nowifi_celebrating(const ui_nowifi_t *nw) { return nw && (nw->mode == M_CELEB || nw->mode == M_FADE); }
