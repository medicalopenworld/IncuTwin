/* Herramientas de revision de la pantalla por la consola serie (cambio tienda): capturas y
 * navegacion de la tienda sin tocar el panel. Se usan con tools/screenshot.py. */
#pragma once

#include <stdbool.h>

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

/* Va a una pantalla para revisarla (capturas del estilo): "inicio", "avisos", "corazones",
 * "historial", "ajustes", "bebe", "mapa" o "racha" (paginas de Inicio, si se pueden ver), "listo"
 * (pulsa «Coge mi mano»), "dedo" / "soltar" (el dedo sobre el bebe) o "fin" (cierra el gesto).
 * 0 si va; 1 si no entiende el destino; 2 si la pantalla esta ocupada. */
int ui_debug_goto(const char *where);

#ifdef __cplusplus
}
#endif
