/* Telemetria: cola persistente en flash (tel_queue) con un doble de flash en RAM.
 * El doble imita la flash de verdad: borrar pone 0xFF por sectores de 4096 y escribir solo baja
 * bits (AND), asi que reescribir sin borrar se nota. Puede simular un corte de luz a mitad de una
 * escritura (`budget`). La cola grande (1,25 MB) va en PSRAM. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "tel_queue.h"
#include "unity.h"

typedef struct {
    uint8_t *mem;
    uint32_t size;
    int32_t budget;      /* bytes que aun se escriben antes del "corte de luz"; < 0 sin limite */
    uint32_t bad_writes; /* escrituras que querian subir bits 0 -> 1 (sin borrar antes) */
    uint32_t erases;
    uint32_t reads;
    uint32_t oob;           /* accesos fuera del doble */
    uint32_t fail_writes;   /* las siguientes N escrituras devuelven false sin escribir nada */
    uint32_t silent_writes; /* las siguientes N devuelven true sin escribir nada */
} tel_flash_ram_t;

static bool tel_flash_ram_read(void *ctx, uint32_t off, void *buf, size_t len)
{
    tel_flash_ram_t *r = ctx;
    r->reads++;
    if ((uint64_t)off + len > r->size) {
        r->oob++;
        return false;
    }
    memcpy(buf, r->mem + off, len);
    return true;
}

static bool tel_flash_ram_write(void *ctx, uint32_t off, const void *buf, size_t len)
{
    tel_flash_ram_t *r = ctx;
    if ((uint64_t)off + len > r->size) {
        r->oob++;
        return false;
    }
    if (r->fail_writes) {
        r->fail_writes--;
        return false;
    }
    if (r->silent_writes) {
        r->silent_writes--;
        return true;
    }
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
    /* Corte de luz escribiendo un evento: a mitad de un sector y justo al empezar uno nuevo; con la
     * cabecera a medias (10 B) o entera pero sin el evento (100 B). */
    const uint32_t before[] = { 5, 16 };
    const int32_t budget[] = { 10, 100 };
    for (int c = 0; c < 4; c++) {
        tel_flash_ram_t r;
        tel_flash_t f = tel_flash_ram_new(&r, 8);
        static tel_queue_t q;
        tel_queue_open(&q, &f, 0);
        uint32_t n = before[c % 2];
        push_n(&q, n);
        r.budget = budget[c / 2];
        tel_event_t e = EVQ(n + 1, "screen");
        TEST_ASSERT_FALSE(tel_queue_push(&q, &e));
        r.budget = -1;

        tel_queue_open(&q, &f, 0); /* arranque tras el corte */
        expect_pending_run(&q, 1, n);
        /* con la cabecera entera su seq ya cuenta: un hueco de seq, nunca uno repetido */
        uint32_t next = budget[c / 2] >= 24 ? n + 2 : n + 1;
        TEST_ASSERT_EQUAL_UINT32(next, tel_queue_next_seq(&q));
        push_n(&q, 3); /* la cola sigue sana: no escribe encima del registro roto */
        for (int k = 0; k < 2; k++) {
            static tel_event_t ev[24];
            TEST_ASSERT_EQUAL_UINT32(n + 3, tel_queue_pending(&q));
            TEST_ASSERT_EQUAL_INT((int)n + 3, tel_queue_peek(&q, ev, 24));
            for (uint32_t i = 0; i < n; i++) TEST_ASSERT_EQUAL_UINT32(i + 1, ev[i].seq);
            for (uint32_t i = 0; i < 3; i++) TEST_ASSERT_EQUAL_UINT32(next + i, ev[n + i].seq);
            tel_queue_open(&q, &f, 0);
        }
        TEST_ASSERT_EQUAL_UINT32(next + 3, tel_queue_next_seq(&q));
        TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
        free(r.mem);
    }
}

TEST_CASE("telq: doble fallo de escritura no deja pendientes fantasma", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    static tel_event_t ev[16];
    tel_queue_open(&q, &f, 0);
    push_n(&q, 5);
    r.fail_writes = 1; /* devuelve false sin escribir */
    tel_event_t e = EVQ(tel_queue_next_seq(&q), "screen");
    TEST_ASSERT_FALSE(tel_queue_push(&q, &e));
    r.silent_writes = 1; /* dice que si pero no escribe: lo pilla la relectura */
    e = EVQ(tel_queue_next_seq(&q), "screen");
    TEST_ASSERT_FALSE(tel_queue_push(&q, &e));
    r.budget = 100; /* y luego un corte a medias */
    e = EVQ(tel_queue_next_seq(&q), "screen");
    TEST_ASSERT_FALSE(tel_queue_push(&q, &e));
    r.budget = -1;
    TEST_ASSERT_EQUAL_UINT32(5, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_UINT32(9, tel_queue_next_seq(&q));

    tel_queue_open(&q, &f, 0);
    TEST_ASSERT_EQUAL_UINT32(5, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_UINT32(9, tel_queue_next_seq(&q)); /* el 8 dejo su cabecera */
    push_n(&q, 3);
    tel_queue_open(&q, &f, 0);
    TEST_ASSERT_EQUAL_UINT32(8, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(8, tel_queue_peek(&q, ev, 16));
    const uint32_t want[8] = { 1, 2, 3, 4, 5, 9, 10, 11 };
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_UINT32(want[i], ev[i].seq);
    tel_queue_ack_through(&q, 11);
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q));
    tel_queue_open(&q, &f, 0);
    TEST_ASSERT_EQUAL_UINT32(0, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_UINT32(12, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

/* Lo que escribiria otra version del firmware: cabecera v1 con otro `len`/`ver`. */
static uint32_t crc32_ref(const void *buf, size_t len)
{
    const uint8_t *p = buf;
    uint32_t c = 0xFFFFFFFFu;
    while (len--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void raw_put(tel_flash_ram_t *r, uint32_t slot, uint32_t seq, uint16_t ver,
                    const uint8_t *payload, uint16_t len)
{
    uint8_t *p = r->mem + slot * TEL_QUEUE_REC;
    memset(p, 0xFF, TEL_QUEUE_REC);
    uint32_t magic = TEL_QUEUE_MAGIC, done = 0xFFFFFFFFu;
    memcpy(p + 0, &magic, 4);
    memcpy(p + 4, &seq, 4);
    memcpy(p + 8, &len, 2);
    memcpy(p + 10, &ver, 2);
    uint32_t hcrc = crc32_ref(p, 12), pcrc = crc32_ref(payload, len);
    memcpy(p + 12, &hcrc, 4);
    memcpy(p + 16, &pcrc, 4);
    memcpy(p + 20, &done, 4);
    memcpy(p + 24, payload, len);
}

TEST_CASE("telq: registros de otra version cuentan para seq y se leen si se puede", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static uint8_t pl[232];
    tel_event_t e;

    /* hueco 0: ver 1 mas corto (una version con `d` de 128 B) -> se rellena con ceros */
    e = EVQ(1, "boot");
    memcpy(pl, &e, sizeof e);
    raw_put(&r, 0, 1, TEL_QUEUE_EV_VER, pl, (uint16_t)(sizeof e - 32));
    /* hueco 1: ver 1 mas largo (un campo nuevo al final) -> se recorta, `d` intacto */
    e = EVQ(2, "screen");
    memcpy(pl, &e, sizeof e);
    memset(pl + sizeof e, 0xA5, sizeof pl - sizeof e);
    raw_put(&r, 1, 2, TEL_QUEUE_EV_VER, pl, sizeof pl);
    /* hueco 2: ver 1 mas largo con un `d` que ya no cabe -> "{}" en vez de JSON cortado */
    e = EVQ(3, "screen");
    memcpy(pl, &e, sizeof e);
    memset(pl + offsetof(tel_event_t, d), 'x', sizeof pl - offsetof(tel_event_t, d));
    pl[sizeof pl - 1] = '\0';
    raw_put(&r, 2, 3, TEL_QUEUE_EV_VER, pl, sizeof pl);
    /* hueco 3: ver 2 (otro formato) -> no se lee, pero su seq manda */
    memset(pl, 0x5A, sizeof pl);
    raw_put(&r, 3, 900, 2, pl, 100);

    static tel_queue_t q;
    tel_queue_open(&q, &f, 0); /* p. ej. tras un restablecimiento de fabrica con la NVS borrada */
    TEST_ASSERT_EQUAL_UINT32(901, tel_queue_next_seq(&q));
    static tel_event_t ev[8];
    TEST_ASSERT_EQUAL_UINT32(3, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(3, tel_queue_peek(&q, ev, 8));
    TEST_ASSERT_EQUAL_UINT32(1, ev[0].seq);
    TEST_ASSERT_EQUAL_STRING("boot", ev[0].kind);
    TEST_ASSERT_EQUAL_UINT32(3, ev[0].up);
    TEST_ASSERT_EQUAL_STRING("{\"p\":1}", ev[0].d);
    TEST_ASSERT_EQUAL_UINT32(2, ev[1].seq);
    TEST_ASSERT_EQUAL_UINT32(6, ev[1].up);
    TEST_ASSERT_EQUAL_STRING("{\"p\":2}", ev[1].d);
    TEST_ASSERT_EQUAL_UINT32(3, ev[2].seq);
    TEST_ASSERT_EQUAL_STRING("{}", ev[2].d);

    tel_event_t n = EVQ(tel_queue_next_seq(&q), "screen");
    TEST_ASSERT_TRUE(tel_queue_push(&q, &n)); /* va tras el de ver 2, sin pisarlo */
    tel_queue_open(&q, &f, 0);
    TEST_ASSERT_EQUAL_UINT32(4, tel_queue_pending(&q));
    TEST_ASSERT_EQUAL_INT(4, tel_queue_peek(&q, ev, 8));
    TEST_ASSERT_EQUAL_UINT32(901, ev[3].seq);
    TEST_ASSERT_EQUAL_UINT32(902, tel_queue_next_seq(&q));
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: abrir la cola llena es rapido y no pasa de 1,25 MB", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, TEL_QUEUE_BYTES / TEL_QUEUE_SECTOR);
    f.size = 0x7D0000; /* la pega pasa la particion `storage` entera */
    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    push_n(&q, TEL_QUEUE_CAP);
    r.reads = 0;
    int64_t t0 = esp_timer_get_time();
    tel_queue_open(&q, &f, 0);
    int64_t dt = esp_timer_get_time() - t0;
    printf("telq: abrir %u huecos con %u pendientes: %lld us, %u lecturas\n",
           (unsigned)(TEL_QUEUE_BYTES / TEL_QUEUE_REC), (unsigned)tel_queue_pending(&q), (long long)dt,
           (unsigned)r.reads);
    TEST_ASSERT_EQUAL_UINT32(TEL_QUEUE_CAP, tel_queue_pending(&q));
    TEST_ASSERT_TRUE(r.reads <= TEL_QUEUE_BYTES / 1024); /* una lectura cada 4 huecos */
    TEST_ASSERT_EQUAL_UINT32(0, r.oob);
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
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

/* NVS borrada y cola viva: gen y boot no pueden volver atras (juntarian duenos y reusarian el ancla
 * de un arranque viejo en el recolector). La cola da los mas altos, confirmados incluidos. */
TEST_CASE("telq: gen y boot mas altos de la cola tras borrar la NVS", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static tel_queue_t q;
    uint32_t g = 99, b = 99;
    tel_queue_open(&q, &f, 0);
    tel_queue_max_ids(&q, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(0, g); /* cola vacia */
    TEST_ASSERT_EQUAL_UINT32(0, b);

    const uint32_t ids[][2] = { { 2, 40 }, { 3, 41 }, { 3, 57 }, { 3, 12 } };
    for (int i = 0; i < 4; i++) {
        tel_event_t e = EVQ(tel_queue_next_seq(&q), "screen");
        e.gen = ids[i][0];
        e.boot = ids[i][1];
        TEST_ASSERT_TRUE(tel_queue_push(&q, &e));
    }
    tel_queue_max_ids(&q, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(3, g);
    TEST_ASSERT_EQUAL_UINT32(57, b);

    tel_queue_ack_through(&q, tel_queue_next_seq(&q) - 1); /* confirmados: siguen contando */
    tel_queue_open(&q, &f, 0);                             /* NVS borrada: acked 0 */
    tel_queue_max_ids(&q, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(3, g);
    TEST_ASSERT_EQUAL_UINT32(57, b);

    /* lo que diria la NVS recien borrada: gen 1, boot_n 1 -> gen 3, boot 58 */
    uint32_t gen = 1, boot = 1;
    TEST_ASSERT_TRUE(tel_ids_floor(&gen, &boot, g, b));
    TEST_ASSERT_EQUAL_UINT32(3, gen);
    TEST_ASSERT_EQUAL_UINT32(58, boot);
    TEST_ASSERT_EQUAL_UINT32(0, r.bad_writes);
    free(r.mem);
}

TEST_CASE("telq: registro corto o con otro `d` no da gen ni boot", "[telemetry]") {
    tel_flash_ram_t r;
    tel_flash_t f = tel_flash_ram_new(&r, 8);
    static uint8_t pl[232];
    tel_event_t e;
    /* ver 1 de antes de gen/boot (192 B): se lee con 0 */
    e = EVQ(1, "boot");
    e.gen = 7; e.boot = 70;
    memcpy(pl, &e, sizeof e);
    raw_put(&r, 0, 1, TEL_QUEUE_EV_VER, pl, (uint16_t)offsetof(tel_event_t, gen));
    /* ver 1 con un `d` mas largo: los bytes donde iria gen/boot son de `d` */
    e = EVQ(2, "screen");
    e.gen = 9; e.boot = 90;
    memcpy(pl, &e, sizeof e);
    memset(pl + offsetof(tel_event_t, d), 'x', sizeof pl - offsetof(tel_event_t, d));
    pl[sizeof pl - 1] = '\0';
    raw_put(&r, 1, 2, TEL_QUEUE_EV_VER, pl, sizeof pl);
    /* y uno normal */
    e = EVQ(3, "screen");
    e.gen = 2; e.boot = 5;
    memcpy(pl, &e, sizeof e);
    raw_put(&r, 2, 3, TEL_QUEUE_EV_VER, pl, sizeof e);

    static tel_queue_t q;
    tel_queue_open(&q, &f, 0);
    uint32_t g = 0, b = 0;
    tel_queue_max_ids(&q, &g, &b);
    TEST_ASSERT_EQUAL_UINT32(2, g);
    TEST_ASSERT_EQUAL_UINT32(5, b);
    free(r.mem);
}

TEST_CASE("tel: tel_ids_floor solo sube", "[telemetry]") {
    uint32_t gen = 4, boot = 100;
    TEST_ASSERT_FALSE(tel_ids_floor(&gen, &boot, 0, 0));        /* cola vacia */
    TEST_ASSERT_FALSE(tel_ids_floor(&gen, &boot, 4, 99));       /* NVS al dia: boot ya es 99 + 1 */
    TEST_ASSERT_EQUAL_UINT32(4, gen);
    TEST_ASSERT_EQUAL_UINT32(100, boot);
    TEST_ASSERT_FALSE(tel_ids_floor(&gen, &boot, 2, 50));       /* la NVS manda si va por delante */
    TEST_ASSERT_TRUE(tel_ids_floor(&gen, &boot, 4, 100));       /* boot repetido: uno mas */
    TEST_ASSERT_EQUAL_UINT32(101, boot);
    TEST_ASSERT_TRUE(tel_ids_floor(&gen, &boot, 6, 0));         /* solo gen */
    TEST_ASSERT_EQUAL_UINT32(6, gen);
    TEST_ASSERT_EQUAL_UINT32(101, boot);
    TEST_ASSERT_FALSE(tel_ids_floor(&gen, &boot, 0, UINT32_MAX)); /* sin desbordar */
}
