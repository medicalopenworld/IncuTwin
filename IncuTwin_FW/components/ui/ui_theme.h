/* Colores del estilo «IncuTwin de bolsillo», los mismos que la app (incutwin-app, theme.ts `K`,
 * docs/diseno-app.md §5.1), y los de estado, que no cambian. Nunca rojo: nada que parezca una
 * alarma. */
#pragma once

#include "lvgl.h"

#define COL_INK        lv_color_hex(0x1F2A5A) /* tinta: texto, bordes y sombras duras  */
#define COL_MUTED      lv_color_hex(0x5E6280) /* texto secundario                       */
#define COL_FAINT      lv_color_hex(0xB9B3C6) /* corazones vacios, puntos inactivos      */
#define COL_LINE       lv_color_hex(0xE6DCE3) /* separadores finos dentro de tarjetas    */
#define COL_TAB_ON     lv_color_hex(0xF1E2EA) /* pastilla de la pestaña activa           */
#define COL_STAGE      lv_color_hex(0xEAF2FB) /* pantallita del bebe (fondo de serie)    */

/* La tinta hace hoy de azul de los textos y marcos: el azul de la marca (0x054E92) queda en el
 * logotipo, que es una imagen. */
#define COL_NAVY       COL_INK
#define COL_NAVY_DARK  COL_INK
#define COL_CORAL      lv_color_hex(0xDE6F63) /* marca: boton principal, corazones       */
#define COL_CORAL_SOFT lv_color_hex(0xFBE3DF) /* fondos suaves coral (agradecimiento)    */
#define COL_CORAL_INK  lv_color_hex(0xB04E43) /* texto coral pequeño (precios)          */
#define COL_BG         lv_color_hex(0xF7EEF3) /* fondo de pantalla, rosa lavanda         */
#define COL_CARD       lv_color_hex(0xFFFDF8) /* tarjetas, etiquetas, barra de abajo     */
#define COL_BG_PHOTO   lv_color_hex(0xE8F1FB) /* fondo durante fototerapia    */

#define COL_OK         lv_color_hex(0x3F9E63) /* verde: bien                  */
#define COL_WARM       lv_color_hex(0xF5A93B) /* ambar: calentando            */
#define COL_PHOTO      lv_color_hex(0x5F9CDE) /* azul: fototerapia            */
#define COL_OFFLINE    lv_color_hex(0x9AA1AA) /* gris: apagado / sin datos    */

#define COL_HALO_CALM  lv_color_hex(0xBBD8F2)
#define COL_HALO_WARM  lv_color_hex(0xF7D9A6)
#define COL_HALO_PHOTO lv_color_hex(0x8FBBF0)
#define COL_HALO_CALL  lv_color_hex(0xF6C3BC) /* coral suave: un bebe te llama */
#define COL_SUPER      lv_color_hex(0xE0A200) /* dorado: super corazon (3 min de mano) */
#define COL_REST       lv_color_hex(0x9CC3E6) /* azul claro: dia sin bebes, no rompe la racha */
