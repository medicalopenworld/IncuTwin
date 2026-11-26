/* Telemetria: cola persistente en flash (tel_queue) con un doble de flash en RAM.
 * El doble imita la flash de verdad: borrar pone 0xFF por sectores de 4096 y escribir solo baja
 * bits (AND), asi que reescribir sin borrar se nota. Puede simular un corte de luz a mitad de una
 * escritura (`budget`). La cola grande (1,25 MB) va en PSRAM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "tel_queue.h"
#include "unity.h"

typedef struct {
    uint8_t *mem;
    uint32_t size;
    int32_t budget;      /* bytes que aun se escriben antes del "corte de luz"; < 0 sin limite */
    uint32_t bad_writes; /* escrituras que querian subir bits 0 -> 1 (sin borrar antes) */
    uint32_t erases;
} tel_flash_ram_t;

static bool tel_flash_ram_read(void *ctx, uint32_t off, void *buf, size_t len)
{
    tel_flash_ram_t *r = ctx;
    if ((uint64_t)off + len > r->size) return false;
    memcpy(buf, r->mem + off, len);
    return true;
}

static bool tel_flash_ram_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
    tel_flash_ram_t *r = ctx;
    if ((uint64_t)off + len > r->size) return false;
    const uint8_t *s = buf;
    for (size_t i = 0; i < len; i++) {
        if (r->budget == 0) return false; /* corte de luz: lo demas no llega */
        if (r->budget > 0) r->budget--;
        if ((r->mem[off + i] & s[i]) != s[i]) r->bad_writes++;
        r->mem[off + i] &= s[i];
    }
    return true;
}

static bool tel_flash_ram_erase(void *ctx, uint32_t off, size_t len)
{
    tel_flash_ram_t *r = ctx;
    if (off % TEL_QUEUE_SECTOR || len % TEL_QUEUE_SECTOR || (uint64_t)off + len > r->size) return false;
    memset(r->mem + off, 0xFF, len);
    r->erases++;
    return true;
}

static void *big_alloc(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n);
    TEST_ASSERT_NOT_NULL_MESSAGE(p, "sin memoria para el doble de flash");
    return p;
}

/* Flash de `sectors` sectores, recien salida de fabrica (0xFF). */
static tel_flash_t tel_flash_ram_new(tel_flash_ram_t *r, uint32_t sectors)
{
    memset(r, 0, sizeof *r);
    r->size = sectors * TEL_QUEUE_SECTOR;
    r->mem = big_alloc(r->size);
    memset(r->mem, 0xFF, r->size);
    r->budget = -1;
    tel_flash_t f = {
        .read = tel_flash_ram_read, .write = tel_flash_ram_write, .erase = tel_flash_ram_erase,
        .size = r->size, .ctx = r,
    };
    return f;
}

static tel_event_t EVQ(uint32_t seq, const char *kind)
{
    tel_event_t e = { .seq = seq, .t = seq > 100 ? 1790000000LL + seq : 0, .up = seq * 3 };
    strncpy(e.kind, kind, sizeof e.kind - 1);
    snprintf(e.d, sizeof e.d, "{\"p\":%u}", (unsigned)(seq % 7));
    return e;
}

/* Mete n eventos con el seq que da la cola. */
static void push_n(tel_queue_t *q, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        tel_event_t e = EVQ(tel_queue_next_seq(q), "screen");
        TEST_ASSERT_TRUE(tel_queue_push(q, &e));
    }
}

/* Lee todos los pendientes y comprueba que son first, first+1, ... sin huecos. */
static void expect_pending_run(const tel_queue_t *q, uint32_t first, uint32_t n)
{
    TEST_ASSERT_EQUAL_UINT32(n, tel_queue_pending(q));
    tel_event_t *ev = big_alloc((n + 1) * sizeof *ev);
    int got = tel_queue_peek(q, ev, (int)n + 1);
    TEST_ASSERT_EQUAL_INT((int)n, got);
    for (uint32_t i = 0; i < n; i++) {
        tel_event_t want = EVQ(first + i, "screen");
        TEST_ASSERT_EQUAL_UINT32(first + i, ev[i].seq);
        TEST_ASSERT_TRUE(want.t == ev[i].t); /* sin soporte de 64 bits en Unity */
        TEST_ASSERT_EQUAL_UINT32(want.up, ev[i].up);
        TEST_ASSERT_EQUAL_STRING(want.d, ev[i].d);
    }
    free(ev);
}

TEST_CASE("telq: push, peek y ack", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    TEST_ASSERT_EQUAL_UINT32(1, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q));

    for (uint32_t s = 1; s <= 5; s++) {
        tel_event_t e = EVQ(s, s == 1 ? "boot" : "screen");
        TEST_ASSERT_TRUE(tel_queue_push(&q, &e));
    }
    TEST_ASSERT_EQUAL_UINT32(5, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_UINT32(6, tel_queue_next_seq(&q));

    static tel_event_t ev[5];
    TEST_ASSERT_EQUAL_INT(3, tel_queue_peek(&q, ev, 3));
    TEST_ASSERT_EQUAL_UINT32(1, ev[0].seq);
    TEST_ASSERT_EQUAL_STRING("boot", ev[0].kind);
    TEST_ASSERT_EQUAL_UINT32(3, ev[2].seq);
    TEST_ASSERT_EQUAL_STRING("screen", ev[2].kind);
    TEST_ASSERT_EQUAL_STRING("{\"p\":3}", ev[2].d);
    TEST_ASSERT_EQUAL_UINT32(5, tel_queue_pending(&q)); /* peek no saca nada */

    tel_queue_ack_through(&q, 3);
    TEST_ASSERT_EQUAL_UINT32(2, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(2, tel_queue_peek(&q, ev, 5));
    TEST_ASSERT_EQUAL_UINT32(4, ev[0].seq);
    TEST_ASSERT_EQUAL_UINT32(5, ev[1].seq);

    /* un seq ya usado no entra: romperia el orden */
    tel_event_t old = EVQ(5, "screen");
    TEST_ASSERT_FALSE(tel_queue_push(&q, &old));
    TEST_ASSERT_EQUAL_UINT32(2, tel_queue_pending(&q));

    tel_queue_ack_through(&q, 5);
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(0, tel_queue_peek(&q, ev, 5));
    TEST_ASSERT_EQUAL_UINT32(6, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_lost(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: reabrir conserva pendientes y next_seq", "[telemetry]") {
    /* Sin WiFi tres dias: 300 eventos y dos reinicios; salen los 300 en orden y sin huecos. */
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 32);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 150);
    tel_queue_open(&q, &f, 0); /* reinicio 1 */
    TEST_ASSERT_EQUAL_UINT32(151, tel_queue_next_seq(&q));
    push_n(&q, 150);
    tel_queue_open(&q, &f, 0); /* reinicio 2 */
    TEST_ASSERT_EQUAL_UINT32(301, tel_queue_next_seq(&q));
    expect_pending_run(&q, 1, 300);
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_lost(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: reabrir con acked_seq no devuelve los confirmados", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 50);
    tel_queue_ack_through(&q, 20);
    tel_queue_open(&q, &f, 20); /* reinicio con los 20 confirmados: no se reenvian */
    expect_pending_run(&q, 21, 30);
    TEST_ASSERT_EQUAL_UINT32(51, tel_queue_next_seq(&q));

    /* PUBACK hasta el 30 guardado en NVS pero el reinicio llego antes de marcarlos en flash */
    tel_queue_open(&q, &f, 30);
    expect_pending_run(&q, 31, 20);
    TEST_ASSERT_EQUAL_UINT32(51, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: llena descarta los más viejos", "[telemetry]") {
    /* Cola llena: 5.010 sin conexion con la particion de verdad (1,25 MB) -> salen los 5.000
     * mas recientes y lost = 10. */
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, TEL_QUEUE_BYTES / TEL_QUEUE_SECTOR);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 5010);
    uint32_t pend = tel_queue_pending(&q);
    TEST_ASSERT_TRUE(pend >= 5000);
    TEST_ASSERT_EQUAL_UINT32(5010 - pend, tel_queue_lost(&q));
    tel_event_t ev[1];
    TEST_ASSERT_EQUAL_INT(1, tel_queue_peek(&q, ev, 1));
    TEST_ASSERT_TRUE(ev[0].seq >= 11);
    expect_pending_run(&q, ev[0].seq, pend); /* los ultimos siempre estan, sin huecos */
    TEST_ASSERT_EQUAL_UINT32(10, tel_queue_lost(&q));

    tel_queue_open(&q, &f, 0); /* tras reiniciar, lo descartado sigue descartado */
    expect_pending_run(&q, 11, 5000);
    TEST_ASSERT_EQUAL_UINT32(5011, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_lost(&q)); /* desde que se abrio */
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: llena en flash pequeña borra sectores sin perder de más", "[telemetry]") {
    /* 8 sectores = 128 huecos; siempre queda uno libre para borrar -> caben 112 pendientes */
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 200);
    expect_pending_run(&q, 89, 112);
    TEST_ASSERT_EQUAL_UINT32(88, tel_queue_lost(&q));
    tel_queue_open(&q, &f, 0);
    expect_pending_run(&q, 89, 112);
    TEST_ASSERT_EQUAL_UINT32(201, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: el anillo da vueltas sin perder ni repetir", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    uint32_t acked = 0, want = 1;
    static tel_event_t ev[TEL_BATCH_N]; /* 3,8 KB: no en la pila de main */
    for (uint32_t i = 1; i <= 1000; i++) {
        push_n(&q, 1);
        if (i % 20 == 0) { /* lote de 20 con su PUBACK */
            int n = tel_queue_peek(&q, ev, TEL_BATCH_N);
            TEST_ASSERT_EQUAL_INT(TEL_BATCH_N, n);
            for (int k = 0; k < n; k++) TEST_ASSERT_EQUAL_UINT32(want++, ev[k].seq);
            acked = ev[n - 1].seq;
            tel_queue_ack_through(&q, acked);
        }
        if (i % 97 == 0) tel_queue_open(&q, &f, acked); /* reinicio */
    }
    TEST_ASSERT_EQUAL_UINT32(1001, want);
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_UINT32(1001, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_lost(&q));
    TEST_ASSERT_TRUE(r.erases > 8); /* dio mas de una vuelta */
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: registro a medias se ignora", "[telemetry]") {
    /* Corte de luz escribiendo un evento: a mitad de un sector y justo al empezar uno nuevo. */
    const uint32_t before[] = { 5, 16 };
    for (int c = 0; c < 2; c++) {
        tel_flash_ram_t r;
        tel_flash_t f = tel_flash_ram_new(&r, 8);
        static tel_queue_t q;
        tel_queue_open(&q, &f, 0);
        uint32_t n = before[c];
        push_n(&q, n);
        r.budget = 100; /* el siguiente registro solo llega hasta el byte 100 */
        tel_event_t e = EVQ(n + 1, "screen");
        tel_queue_push(&q, &e);
        r.budget = -1;

        tel_queue_open(&q, &f, 0); /* arranque tras el corte */
        expect_pending_run(&q, 1, n);
        TEST_ASSERT_EQUAL_UINT32(n + 1, tel_queue_next_seq(&q));
        push_n(&q, 3); /* la cola sigue sana: no escribe encima del registro roto */
        expect_pending_run(&q, 1, n + 3);
        tel_queue_open(&q, &f, 0);
        expect_pending_run(&q, 1, n + 3);
        TEST_ASSERT_EQUAL_UINT32(n + 4, tel_queue_next_seq(&q));
        TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
        free(r.mem);
    }
}

TEST_CASE("telq: next_seq tras borrar la NVS", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 64);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 900);
    tel_queue_ack_through(&q, 900);
    tel_queue_open(&q, &f, 0); /* NVS borrada: acked 0, la flash aun tiene seq hasta 900 */
    TEST_ASSERT_EQUAL_UINT32(901, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q)); /* los confirmados no vuelven */

    /* y si la NVS dice mas que la flash, manda la NVS */
    tel_queue_open(&q, &f, 2000);
    TEST_ASSERT_EQUAL_UINT32(2001, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: restablecimiento de fábrica sin conexión conserva el reset", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, 10);
    tel_queue_ack_through(&q, 8);
    tel_event_t e = EVQ(tel_queue_next_seq(&q), "reset");
    TEST_ASSERT_TRUE(tel_queue_push(&q, &e));

    tel_queue_open(&q, &f, 0); /* nuevo dueno: NVS de fabrica, la cola no se borra */
    static tel_event_t ev[5];
    TEST_ASSERT_EQUAL_UINT32(3, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(3, tel_queue_peek(&q, ev, 5));
    TEST_ASSERT_EQUAL_UINT32(9, ev[0].seq);
    TEST_ASSERT_EQUAL_UINT32(10, ev[1].seq);
    TEST_ASSERT_EQUAL_UINT32(11, ev[2].seq);
    TEST_ASSERT_EQUAL_STRING("reset", ev[2].kind);
    TEST_ASSERT_EQUAL_UINT32(12, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}
