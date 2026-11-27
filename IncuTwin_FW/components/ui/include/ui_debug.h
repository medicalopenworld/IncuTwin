/* Herramientas de revision de la pantalla por la consola serie (cambio tienda): capturas y
 * navegacion de la tienda sin tocar el panel. Se usan con tools/screenshot.py. */
#pragma once

#include <stdbool.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Vuelca la pantalla activa (RGB565) y la capa superior (ARGB8888: barra, dialogos) por stdout,
 * comprimidas (RLE) en base64 entre lineas «SNAP BEGIN ...» y «SNAP END». 0 si ha ido bien. */
int ui_debug_screenshot(void);

/* Abre la tienda en una pestaña: "gorros", "mantas", "fondos" o "efectos" (`girl`:
 * gorros de niña). Con `try_item` >= 0 abre Probar de ese rw_item_t; con `buy` ademas el dialogo
 * de comprar (no compra nada hasta confirmar en la pantalla). "cerrar" cierra la tienda y va a
 * Inicio; "corazones" va a la pestaña Corazones. false si la pestaña no existe. */
bool ui_debug_shop(const char *tab, bool girl, int try_item, bool buy);
/* Bebe sin WiFi en Inicio: "on" (nino), "nina" o "off" (vuelve a lo de verdad y, con WiFi,
 * celebra); "asistente" simula la pantalla del asistente probando la WiFi y su celebracion.
 * Devuelve false si no entiende el modo. */
bool ui_debug_nowifi(const char *mode);
/* Bebe de Inicio: "caricia" o "cosquillas" lanzan su reaccion (no cuenta como uso) y, con `secs` > 0,
 * la reinician cada segundo durante `secs` s para poder capturarla; "bostezo" bosteza ya. false si no
 * entiende la orden. */
bool ui_debug_baby(const char *what, int secs);
#if CONFIG_INCUTWIN_DEBUG_TOUCH
/* SOLO PRUEBAS (producto dev): un dedo de mentira que baja en (x0,y0), va a (x1,y1) en `ms` ms (con
 * `laps` > 0 hace `laps` idas y vueltas en ese tiempo) y se levanta. Pasa por el mismo camino que el
 * tactil. false si hay otro trazo en curso o los datos no valen. */
bool ui_debug_touch(int x0, int y0, int x1, int y1, int ms, int laps);
#endif

#ifdef __cplusplus
}
#endif
