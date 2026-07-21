#pragma once

#include <lvgl.h>

/* IncuTwin brand colours (sampled from IncuTwin_logo.png) */
#define COL_NAVY lv_color_hex(0x054E92)      /* wordmark blue            */
#define COL_NAVY_DARK lv_color_hex(0x1E3E6E) /* line-art navy            */
#define COL_CORAL lv_color_hex(0xDE6F63)     /* baby line-art coral      */
#define COL_RED lv_color_hex(0xE0524A)       /* "i" dot red / alerts     */
#define COL_BG lv_color_hex(0xF4F3F0)        /* off-white background     */
#define COL_CARD lv_color_hex(0xFFFFFF)

/* State colours */
#define COL_OK lv_color_hex(0x3F9E63)        /* green: fine              */
#define COL_WARM lv_color_hex(0xF5A93B)      /* amber: heating           */
#define COL_PHOTO lv_color_hex(0x5F9CDE)     /* blue: phototherapy       */
#define COL_OFFLINE lv_color_hex(0x9AA1AA)   /* gray: off / no data      */

/* Halo (breathing light behind the baby) */
#define COL_HALO_CALM lv_color_hex(0xBBD8F2)
#define COL_HALO_WARM lv_color_hex(0xF7D9A6)
#define COL_HALO_PHOTO lv_color_hex(0x8FBBF0)
#define COL_HALO_ALARM lv_color_hex(0xF3B0AA)
