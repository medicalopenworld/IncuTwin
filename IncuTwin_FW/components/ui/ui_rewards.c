/* Recompensas en la UI (spec rewards, twin-display): copia del estado de `rewards` para pintar,
 * colores de los temas (halo en reposo y fondo de Inicio), nombres de niveles, sellos y opciones y
 * el dibujo de cada sello (pagina de Logros y franja). Todo en la tarea de LVGL. */
#include "assets.h"
#include "baby_widget.h"
#include "rewards.h"
#include "ui_internal.h"
#include "ui_toy.h"

static rw_state_t s_rw;
static bool s_demo;
static bool s_have;

/* halo en reposo: Cielo es el de siempre (COL_HALO_CALM) */
static const uint32_t HALO_COLS[RW_HALO_LAST - RW_HALO_FIRST + 1] = {
    0xBBD8F2, /* Cielo */
    0xD8CCF2, /* Lavanda */
    0xBFE8D6, /* Menta */
    0xF8D2BC, /* Melocoton */
    0xE6C3EA, /* Lila */
};
/* fondo de la pantallita de Inicio: Azul bebe es el de serie (COL_STAGE, el de la app) */
static const uint32_t BG_COLS[RW_BG_LAST - RW_BG_FIRST + 1] = {
    0xEAF2FB, /* Azul bebe */
    0xF8F1E2, /* Crema */
    0xFBEDEE, /* Rosa */
    0xE7F4F1, /* Verde agua */
};

void ui_rewards_refresh(void)
{
    rewards_copy(&s_rw, &s_demo);
    s_have = true;
}

const rw_state_t *ui_rewards(void)
{
    if (!s_have) ui_rewards_refresh();
    return &s_rw;
}

lv_color_t ui_item_color(rw_item_t item)
{
    if ((int)item <= RW_HALO_LAST) return lv_color_hex(HALO_COLS[item - RW_HALO_FIRST]);
    if (item >= RW_BG_FIRST && item <= RW_BG_LAST) return lv_color_hex(BG_COLS[item - RW_BG_FIRST]);
    return COL_CORAL; /* guirnalda */
}

lv_color_t ui_theme_bg(void)
{
    const rw_state_t *s = ui_rewards();
    return s->bg >= RW_BG_FIRST && s->bg <= RW_BG_LAST ? ui_item_color((rw_item_t)s->bg) : COL_STAGE;
}

lv_color_t ui_theme_halo_calm(void)
{
    const rw_state_t *s = ui_rewards();
    return s->halo <= RW_HALO_LAST ? ui_item_color((rw_item_t)s->halo) : COL_HALO_CALM; /* RW_HALO_FIRST = 0 */
}

const char *ui_level_name(rw_level_t l)
{
    static const ui_str_t S[RW_LEVEL_COUNT] = { STR_LEVEL_SPARK, STR_LEVEL_WARMTH, STR_LEVEL_HUG,
                                                STR_LEVEL_BLANKET, STR_LEVEL_SUN };
    return l < RW_LEVEL_COUNT ? tr(S[l]) : "";
}

const char *ui_seal_name(rw_seal_t s)
{
    static const ui_str_t S[RW_SEAL_COUNT] = { STR_SEAL_FIRST, STR_SEAL_WEEK, STR_SEAL_MONTH_RUN, STR_SEAL_30_DAYS,
                                               STR_SEAL_SUPER, STR_SEAL_3_BABIES, STR_SEAL_HOME };
    return s < RW_SEAL_COUNT ? tr(S[s]) : "";
}

const char *ui_seal_desc(rw_seal_t s)
{
    static const ui_str_t S[RW_SEAL_COUNT] = { STR_SEAL_FIRST_D, STR_SEAL_WEEK_D, STR_SEAL_MONTH_RUN_D,
                                               STR_SEAL_30_DAYS_D, STR_SEAL_SUPER_D, STR_SEAL_3_BABIES_D,
                                               STR_SEAL_HOME_D };
    return s < RW_SEAL_COUNT ? tr(S[s]) : "";
}

const char *ui_item_name(rw_item_t i)
{
    static const ui_str_t S[RW_ITEM_COUNT] = {
        STR_ITEM_HALO_SKY, STR_ITEM_HALO_LAVENDER, STR_ITEM_HALO_MINT, STR_ITEM_HALO_PEACH, STR_ITEM_HALO_LILAC,
        STR_ITEM_BG_DEFAULT, STR_ITEM_BG_CREAM, STR_ITEM_BG_PINK, STR_ITEM_BG_AQUA, STR_ITEM_GARLAND,
        STR_ITEM_NAMED_INCUNEST,
        STR_ITEM_BASIC_HAT, STR_ITEM_MINT, STR_ITEM_CHICK, STR_ITEM_CLOUD,           /* gorros de niño */
        STR_ITEM_BASIC_HAT, STR_ITEM_LILAC, STR_ITEM_PEACH, STR_ITEM_MINT,           /* gorros de niña */
        STR_ITEM_BASIC_BLANKET, STR_ITEM_CREAM, STR_ITEM_MINT, STR_ITEM_LAVENDER, STR_ITEM_PEACH, /* mantas */
        STR_ITEM_SANTA, STR_ITEM_SNOW,
    };
    return i < RW_ITEM_COUNT ? tr(S[i]) : "";
}

/* ------------------------------------------------------------- ropa del bebe */

uint8_t ui_hat_style(rw_item_t i)
{
    switch (i) {
    case RW_ITEM_HAT_BOY_MINT:
    case RW_ITEM_HAT_GIRL_MINT:   return BABY_HAT_MINT;
    case RW_ITEM_HAT_BOY_CHICK:   return BABY_HAT_CHICK;
    case RW_ITEM_HAT_BOY_CLOUD:   return BABY_HAT_CLOUD;
    case RW_ITEM_HAT_GIRL_LILAC:  return BABY_HAT_LILAC;
    case RW_ITEM_HAT_GIRL_PEACH:  return BABY_HAT_PEACH;
    default:                      return BABY_HAT_BASIC;
    }
}

uint8_t ui_blanket_style(rw_item_t i)
{
    switch (i) {
    case RW_ITEM_BLANKET_CREAM:    return BABY_BLANKET_CREAM;
    case RW_ITEM_BLANKET_MINT:     return BABY_BLANKET_MINT;
    case RW_ITEM_BLANKET_LAVENDER: return BABY_BLANKET_LAVENDER;
    case RW_ITEM_BLANKET_PEACH:    return BABY_BLANKET_PEACH;
    default:                       return BABY_BLANKET_BASIC;
    }
}

void ui_outfit_apply(void)
{
    const rw_state_t *r = ui_rewards();
    int m, d;
    rewards_local_md(&m, &d);
    bool santa = rw_wears(r, RW_ITEM_SANTA, m, d);
    baby_set_outfit(ui_hat_style(rw_hat_for(r, false, 0, 0)), ui_hat_style(rw_hat_for(r, true, 0, 0)),
                    ui_blanket_style((rw_item_t)r->blanket), santa);
}

/* ------------------------------------------------------------- sellos */

static lv_obj_t *seal_text(lv_obj_t *c, const char *t, lv_color_t col, int32_t d)
{
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, d >= 40 ? &lv_font_title_16 : &lv_font_title_14, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_center(l);
    return l;
}

static lv_obj_t *seal_image(lv_obj_t *c, const lv_image_dsc_t *img, uint16_t scale)
{
    lv_obj_t *i = lv_image_create(c);
    lv_image_set_src(i, img);
    if (scale != LV_SCALE_NONE) lv_image_set_scale(i, scale);
    lv_obj_set_clickable(i, false);
    lv_obj_center(i);
    return i;
}

/* Circulo de `d` px con el dibujo del sello: conseguido en color; el que falta, blanco con el
 * borde gris y el dibujo casi transparente */
lv_obj_t *ui_seal_icon(lv_obj_t *parent, rw_seal_t seal, bool earned, int32_t d)
{
    lv_color_t ring = COL_CORAL, fill = COL_CARD;
    switch (seal) {
    case RW_SEAL_WEEK:      ring = fill = COL_NAVY; break;
    case RW_SEAL_MONTH_RUN: ring = fill = COL_CORAL; break;
    case RW_SEAL_30_DAYS:   ring = COL_NAVY; fill = COL_BG_PHOTO; break;
    case RW_SEAL_SUPER:     ring = COL_SUPER; break;
    case RW_SEAL_HOME:      ring = fill = COL_OK; break;
    default: break;
    }
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, d, d);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, earned ? fill : COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, earned ? ring : COL_FAINT, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);

    lv_obj_t *art = NULL;
    int32_t cell = d >= 40 ? 3 : 2; /* corazon de pixeles: 27x24 o 18x16 dentro del circulo */
    switch (seal) {
    case RW_SEAL_FIRST:
        art = toy_heart_img(c, TOY_HEART_FULL, cell);
        lv_obj_center(art);
        break;
    case RW_SEAL_WEEK:
        art = seal_text(c, "7", earned ? lv_color_white() : COL_NAVY, d);
        break;
    case RW_SEAL_MONTH_RUN:
        art = seal_text(c, "30", earned ? lv_color_white() : COL_CORAL, d);
        break;
    case RW_SEAL_30_DAYS:
        art = seal_text(c, "30", COL_NAVY, d);
        break;
    case RW_SEAL_SUPER:
        art = toy_heart_img(c, TOY_HEART_SUPER, cell);
        lv_obj_center(art);
        break;
    case RW_SEAL_3_BABIES:
        art = seal_image(c, &img_baby_head_boy, d >= 40 ? LV_SCALE_NONE : 220);
        break;
    default: /* a casa */
        art = seal_text(c, LV_SYMBOL_HOME, earned ? lv_color_white() : COL_OK, d);
        break;
    }
    if (!earned && art) {
        if (lv_obj_check_type(art, &lv_image_class)) lv_obj_set_style_image_opa(art, LV_OPA_30, 0);
        else lv_obj_set_style_text_opa(art, LV_OPA_30, 0);
    }
    return c;
}
