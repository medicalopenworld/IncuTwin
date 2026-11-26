/* IncuTwin — telemetria de uso: cola persistente de eventos en flash (spec telemetry-events).
 *
 * C puro sobre una interfaz de flash (tel_flash_t): en el firmware la pega con esp_partition va en
 * telemetry.c; en test_apps, un doble en RAM. Sin malloc: tel_queue_t se declara en estatico.
 *
 * Formato (fijo: lo leeran versiones futuras): anillo de registros de 256 B, 16 por sector de
 * 4096 B, sobre los primeros TEL_QUEUE_BYTES de la flash que se pase. Cada registro:
 *   off  0 u32 magia TEL_QUEUE_MAGIC     off 12 u32 CRC32 de la cabecera [0..12)
 *   off  4 u32 seq                       off 16 u32 CRC32 de los `len` bytes del evento
 *   off  8 u16 len (<= 232)              off 20 u32 hecho: 0xFFFFFFFF pendiente; otro valor =
 *   off 10 u16 ver del evento (1)                  confirmado o descartado (se escribe sin borrar)
 *   off 24 evento (`len` bytes; ver 1 = tel_event_t)   resto hasta 256 sin escribir (0xFF)
 * Toda cabecera sana cuenta para el seq mas alto y la posicion de escritura, sea de la version que
 * sea. Solo se envian los de ver 1 con su CRC bien; si `len` no coincide con tel_event_t se rellena
 * con ceros o se recorta (anadir campos al final o crecer `d` no cambia `ver`; cambiar el
 * significado de los bytes si). Un registro a medias por un corte de luz se ignora.
 * Un sector se borra justo antes de reutilizarlo. La cola guarda como mucho TEL_QUEUE_CAP
 * pendientes (menos si la flash es pequena); al pasarse descarta el mas viejo y suma `lost`.
 * No es reentrante: el que llama serializa el acceso (p. ej. con un mutex).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tel_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEL_QUEUE_SECTOR   4096u
#define TEL_QUEUE_REC      256u
#define TEL_QUEUE_CAP      5000u                     /* pendientes como mucho */
#define TEL_QUEUE_BYTES    (320u * TEL_QUEUE_SECTOR) /* 1,25 MB de `storage`: 5.120 huecos */
#define TEL_QUEUE_MAGIC    0x314C4554u               /* "TEL1" */
#define TEL_QUEUE_EV_VER   1u                        /* `ver` de los registros que escribe esta version */

typedef struct {   /* lo que la cola necesita de la flash; esp_partition en telemetry.c, RAM en test */
    bool (*read)(void *ctx, uint32_t off, void *buf, size_t len);
    bool (*write)(void *ctx, uint32_t off, const void *buf, size_t len);
    bool (*erase)(void *ctx, uint32_t off, size_t len);   /* sectores de 4096 */
    uint32_t size;                                        /* bytes usables */
    void *ctx;
} tel_flash_t;

typedef struct { /* privado; tamano fijo para poder declararlo en estatico */
    tel_flash_t f;
    uint32_t nslots;   /* huecos de 256 B (sectores enteros * 16) */
    uint32_t cap;      /* pendientes como mucho */
    uint32_t head;     /* hueco donde va el siguiente registro */
    uint32_t tail;     /* hueco del pendiente mas viejo (= head si no hay) */
    uint32_t pending;
    uint32_t acked;    /* ultimo seq confirmado */
    uint32_t next_seq;
    uint32_t lost;
} tel_queue_t;

/* Reconstruye la cola leyendo la flash; acked_seq = último seq confirmado (NVS). */
void     tel_queue_open(tel_queue_t *q, const tel_flash_t *f, uint32_t acked_seq);
uint32_t tel_queue_next_seq(const tel_queue_t *q);              /* max(seq visto, acked) + 1 */
/* e->seq ya asignado (>= tel_queue_next_seq). false si no se pudo guardar (se registra fuera). */
bool     tel_queue_push(tel_queue_t *q, const tel_event_t *e);
int      tel_queue_peek(const tel_queue_t *q, tel_event_t *out, int max); /* los más viejos sin confirmar */
/* Confirma (y marca en flash) los pendientes con seq <= seq. */
void     tel_queue_ack_through(tel_queue_t *q, uint32_t seq);
uint32_t tel_queue_pending(const tel_queue_t *q);
uint32_t tel_queue_lost(const tel_queue_t *q);                  /* desde que se abrió */

#ifdef __cplusplus
}
#endif
