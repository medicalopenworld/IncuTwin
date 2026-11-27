/* Capturas de pantalla y navegacion de la tienda por la consola (ui_debug.h). */
#include "ui_debug.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/base64.h"
#include "assets_nowifi.h"
#include "ui_internal.h"
#include "ui_nowifi.h"
#include "ui_pages.h"

/* ------------------------------------------------------------- base64 por lineas */

#define B64_IN 57 /* 76 caracteres por linea */

static uint8_t s_chunk[B64_IN];
static size_t s_fill;

static void b64_flush(void)
{
    if (!s_fill) return;
    unsigned char out[80];
    size_t olen = 0;
    mbedtls_base64_encode(out, sizeof(out), &olen, s_chunk, s_fill);
    out[olen] = '\0';
    printf("%s\n", out);
    s_fill = 0;
}

static void b64_put(const uint8_t *p, size_t n)
{
    while (n--) {
        s_chunk[s_fill++] = *p++;
        if (s_fill == B64_IN) b64_flush();
    }
}

/* RLE: [n 1..255][pixel de `bpp` bytes] */
static void dump(const char *name, lv_draw_buf_t *buf, int bpp, const char *fmt)
{
    const lv_image_header_t *h = &buf->header;
    printf("SNAP BEGIN %s %u %u %s\n", name, (unsigned)h->w, (unsigned)h->h, fmt);
    s_fill = 0;
    for (uint32_t y = 0; y < h->h; y++) {
        const uint8_t *row = buf->data + y * h->stride;
        uint32_t x = 0;
        while (x < h->w) {
            const uint8_t *px = row + x * bpp;
            uint8_t n = 1;
            while (x + n < h->w && n < 255 && memcmp(px, row + (x + n) * bpp, bpp) == 0) n++;
            b64_put(&n, 1);
            b64_put(px, bpp);
            x += n;
        }
    }
    b64_flush();
    printf("SNAP END %s\n", name);
}

/* El pintado de la captura necesita la pila de la tarea de LVGL (la consola se queda corta) */
static lv_draw_buf_t *s_snap_scr, *s_snap_top;
static SemaphoreHandle_t s_snap_done;

static void snap_async(void *arg)
{
    (void)arg;
    s_snap_scr = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    s_snap_top = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888);
    xSemaphoreGive(s_snap_done);
}

int ui_debug_screenshot(void)
{
    if (!s_snap_done) s_snap_done = xSemaphoreCreateBinary();
    s_snap_scr = s_snap_top = NULL;
    if (!lvgl_port_lock(3000)) return -1;
    lv_async_call(snap_async, NULL);
    lvgl_port_unlock();
    if (xSemaphoreTake(s_snap_done, pdMS_TO_TICKS(5000)) != pdTRUE) return -3;
    lv_draw_buf_t *scr = s_snap_scr, *top = s_snap_top;
    int err = 0;
    if (scr) dump("scr", scr, 2, "rgb565");
    else err = -2;
    if (top) dump("top", top, 4, "argb8888");
    if (scr) lv_draw_buf_destroy(scr);
    if (top) lv_draw_buf_destroy(top);
    printf("SNAP DONE\n");
    return err;
}

/* Tambien en la tarea de LVGL: crear la tienda (bebes de Probar incluidos) pide pila */
static struct { int tab; bool girl, close, hearts, buy; int item; } s_nav;

static void shop_async(void *arg)
{
    (void)arg;
    if (s_nav.close || s_nav.hearts) {
        ui_shop_debug(-1, false, -1, false);
        ui_go(s_nav.close ? UI_TAB_HOME : UI_TAB_HEARTS);
    } else {
        ui_go(UI_TAB_HEARTS);
        ui_shop_debug(s_nav.tab, s_nav.girl, s_nav.item, s_nav.buy);
    }
}

bool ui_debug_shop(const char *tab, bool girl, int try_item, bool buy)
{
    static const char *const TABS[] = { "gorros", "mantas", "fondos", "efectos" };
    int t = -1;
    for (int i = 0; i < 4; i++) {
        if (strcmp(tab, TABS[i]) == 0) t = i;
    }
    bool close = strcmp(tab, "cerrar") == 0, hearts = strcmp(tab, "corazones") == 0;
    if (t < 0 && !close && !hearts) return false;
    if (!lvgl_port_lock(3000)) return false;
    s_nav.tab = t;
    s_nav.girl = girl;
    s_nav.close = close;
    s_nav.hearts = hearts;
    s_nav.buy = buy;
    s_nav.item = try_item;
    lv_async_call(shop_async, NULL);
    lvgl_port_unlock();
    return true;
}

/* ------------------------------------------------------------- bebe sin WiFi */

static void nowifi_async(void *arg) { ui_home_nowifi_force((int8_t)(intptr_t)arg); }

/* `sinwifi asistente`: la pantalla del asistente mientras prueba la WiFi (busca 5 s), la
 * celebracion y vuelta a la pantalla de antes; asi se ve sin borrar la WiFi */
static lv_obj_t *s_wiz_scr, *s_wiz_prev;
static ui_nowifi_t *s_wiz_nw;

static void wiz_back(void *arg)
{
    (void)arg;
    lv_screen_load(s_wiz_prev);
    lv_obj_delete(s_wiz_scr); /* y con ella la instancia del bebe */
    s_wiz_scr = NULL;
}

static void wiz_done(void) { lv_async_call(wiz_back, NULL); }

static void wiz_timer(lv_timer_t *t)
{
    (void)t;
    ui_nowifi_celebrate(s_wiz_nw, false, wiz_done);
}

static void wizard_async(void *arg)
{
    (void)arg;
    if (s_wiz_scr) return;
    s_wiz_prev = lv_screen_active();
    s_wiz_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_wiz_scr, COL_BG, 0);
    lv_obj_set_scrollable(s_wiz_scr, false);
    s_wiz_nw = ui_nowifi_create(s_wiz_scr, (240 - NOWIFI_W) / 2, 40, 40 + NOWIFI_H + 8);
    ui_nowifi_search(s_wiz_nw, false);
    lv_screen_load(s_wiz_scr);
    lv_timer_t *t = lv_timer_create(wiz_timer, 5000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

bool ui_debug_nowifi(const char *mode)
{
    if (strcmp(mode, "asistente") == 0) {
        if (!lvgl_port_lock(3000)) return false;
        lv_async_call(wizard_async, NULL);
        lvgl_port_unlock();
        return true;
    }
    int m = strcmp(mode, "on") == 0 ? 1 : strcmp(mode, "nina") == 0 ? 2 : strcmp(mode, "off") == 0 ? 0 : -1;
    if (m < 0 || !lvgl_port_lock(3000)) return false;
    lv_async_call(nowifi_async, (void *)(intptr_t)m);
    lvgl_port_unlock();
    return true;
}

/* ------------------------------------------------------------- ir a una pantalla */

static void goto_async(void *arg)
{
    int w = (int)(intptr_t)arg;
    if (w < UI_TAB_COUNT) {
        ui_shop_debug(-1, false, -1, false); /* la tienda tapa Corazones */
        ui_go((ui_tab_t)w);
        return;
    }
    ui_go(UI_TAB_HOME);
    switch (w) {
    case 10: ui_pages_goto(UI_PAGE_BABY); break;
    case 11: ui_pages_goto(UI_PAGE_MAP); break;
    case 12: ui_pages_goto(UI_PAGE_STREAK); break;
    default: ui_home_debug_hand(w - 20); break;
    }
}

bool ui_debug_goto(const char *where)
{
    static const char *const W[] = { "inicio", "avisos", "corazones", "historial", "ajustes" };
    static const char *const P[] = { "bebe", "mapa", "racha" };
    static const char *const H[] = { "listo", "dedo", "soltar", "fin" };
    int w = -1;
    for (int i = 0; i < 5; i++) {
        if (strcmp(where, W[i]) == 0) w = i;
    }
    for (int i = 0; i < 3; i++) {
        if (strcmp(where, P[i]) == 0) w = 10 + i;
    }
    for (int i = 0; i < 4; i++) {
        if (strcmp(where, H[i]) == 0) w = 20 + i;
    }
    if (w < 0 || !lvgl_port_lock(3000)) return false;
    lv_async_call(goto_async, (void *)(intptr_t)w);
    lvgl_port_unlock();
    return true;
}
