/* IncuTwin — cola persistente de eventos de telemetria (ver tel_queue.h para el formato).
 *
 * Invariantes:
 *  - Los registros se escriben en orden de hueco y con seq creciente; los pendientes son los
 *    registros sanos, sin marca de hecho y con seq > acked, entre tail y head.
 *  - Un sector se borra justo cuando head entra en el. Como pending <= cap <= (sectores - 1) * 16,
 *    el sector que se borra no tiene pendientes (salvo huecos rotos acumulados por cortes; entonces
 *    sus pendientes se cuentan en lost).
 *  - Lo descartado y lo confirmado se marca en flash (palabra `hecho` a 0, sin borrar), asi que
 *    no vuelve tras reiniciar aunque se pierda la NVS (restablecimiento de fabrica).
 */
#include "tel_queue.h"

#include <stddef.h>
#include <string.h>

#define SPS       (TEL_QUEUE_SECTOR / TEL_QUEUE_REC) /* huecos por sector: 16 */
#define REC_VER   1u
#define DONE_OFF  16u
#define NOT_DONE  0xFFFFFFFFu

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint16_t len;
    uint16_t ver;
    uint32_t crc;  /* de seq, len, ver y el evento */
    uint32_t done; /* NOT_DONE = pendiente */
    uint32_t rsv;
} rec_hdr_t;

typedef struct {
    rec_hdr_t h;
    tel_event_t e;
} rec_t;

_Static_assert(sizeof(rec_hdr_t) == 24, "cabecera de 24 B");
_Static_assert(offsetof(rec_hdr_t, done) == DONE_OFF, "palabra hecho en el byte 16");
_Static_assert(offsetof(rec_t, e) == sizeof(rec_hdr_t), "evento justo tras la cabecera");
_Static_assert(sizeof(rec_t) <= TEL_QUEUE_REC, "el registro cabe en 256 B");

/* CRC-32 (IEEE, reflejado, el de zlib) con tabla de 16 entradas: pequeno y portable. */
static uint32_t crc32_upd(uint32_t crc, const void *buf, size_t len)
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    const uint8_t *p = buf;
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ T[crc & 15];
        crc = (crc >> 4) ^ T[crc & 15];
    }
    return ~crc;
}

static uint32_t rec_crc(const rec_t *r)
{
    uint32_t c = crc32_upd(0, &r->h.seq, offsetof(rec_hdr_t, crc) - offsetof(rec_hdr_t, seq));
    return crc32_upd(c, &r->e, sizeof r->e);
}

static uint32_t nxt(const tel_queue_t *q, uint32_t s)
{
    return s + 1 == q->nslots ? 0 : s + 1;
}

/* Lee el hueco; true si tiene un registro sano (magia, longitud y CRC). */
static bool slot_read(const tel_queue_t *q, uint32_t slot, rec_t *r)
{
    uint32_t off = slot * TEL_QUEUE_REC;
    if (!q->f.read(q->f.ctx, off, &r->h, sizeof r->h)) return false;
    if (r->h.magic != TEL_QUEUE_MAGIC || r->h.len != sizeof(tel_event_t) || r->h.ver != REC_VER) return false;
    if (!q->f.read(q->f.ctx, off + sizeof r->h, &r->e, sizeof r->e)) return false;
    if (rec_crc(r) != r->h.crc || r->e.seq != r->h.seq) return false;
    r->e.kind[TEL_KIND_MAX - 1] = '\0';
    r->e.d[TEL_D_MAX - 1] = '\0';
    return true;
}

static bool live(const tel_queue_t *q, const rec_t *r)
{
    return r->h.done == NOT_DONE && r->h.seq > q->acked;
}

static bool slot_blank(const tel_queue_t *q, uint32_t slot)
{
    uint8_t buf[64];
    for (uint32_t o = 0; o < TEL_QUEUE_REC; o += sizeof buf) {
        if (!q->f.read(q->f.ctx, slot * TEL_QUEUE_REC + o, buf, sizeof buf)) return false;
        for (size_t i = 0; i < sizeof buf; i++)
            if (buf[i] != 0xFF) return false;
    }
    return true;
}

/* Marca el registro como hecho (baja bits, no hace falta borrar). Si falla, al reabrir lo cubre
 * acked_seq (confirmados) o vuelve a descartarse (descartados). */
static void mark_done(tel_queue_t *q, uint32_t slot)
{
    const uint32_t zero = 0;
    (void)q->f.write(q->f.ctx, slot * TEL_QUEUE_REC + DONE_OFF, &zero, sizeof zero);
}

/* tail = primer pendiente desde `from` (o head si no queda ninguno). */
static void seek_tail(tel_queue_t *q, uint32_t from)
{
    rec_t r;
    uint32_t s = from;
    while (q->pending && s != q->head) {
        if (slot_read(q, s, &r) && live(q, &r)) break;
        s = nxt(q, s);
    }
    q->tail = q->pending ? s : q->head;
}

/* Descarta el pendiente mas viejo y lo suma a lost. */
static void drop_tail(tel_queue_t *q, bool mark)
{
    if (mark) mark_done(q, q->tail);
    q->pending--;
    q->lost++;
    seek_tail(q, nxt(q, q->tail));
}

void tel_queue_open(tel_queue_t *q, const tel_flash_t *f, uint32_t acked_seq)
{
    if (!q) return;
    memset(q, 0, sizeof *q);
    q->acked = acked_seq;
    q->next_seq = acked_seq + 1;
    if (!f || !f->read || !f->write || !f->erase) return; /* cap 0: push siempre false */
    q->f = *f;
    uint32_t sectors = f->size / TEL_QUEUE_SECTOR;
    if (sectors < 2) return;
    q->nslots = sectors * SPS;
    q->cap = (sectors - 1) * SPS;
    if (q->cap > TEL_QUEUE_CAP) q->cap = TEL_QUEUE_CAP;

    /* 1) el registro sano con el seq mas alto marca donde se escribio por ultimo */
    rec_t r;
    bool any = false;
    uint32_t max_seq = 0, max_slot = 0;
    for (uint32_t s = 0; s < q->nslots; s++) {
        if (slot_read(q, s, &r) && (!any || r.h.seq > max_seq)) {
            any = true;
            max_seq = r.h.seq;
            max_slot = s;
        }
    }
    if (max_seq > acked_seq) q->next_seq = max_seq + 1;
    q->head = any ? nxt(q, max_slot) : 0;

    /* 2) un registro a medias (corte de luz) no se puede reescribir sin borrar: se salta. Al
     *    principio de sector no hace falta, el sector se borra antes de escribir. */
    while (q->head % SPS != 0 && !slot_blank(q, q->head)) q->head = nxt(q, q->head);

    /* 3) pendientes, del mas viejo (justo tras head) al mas nuevo */
    q->tail = q->head;
    uint32_t s = q->head;
    for (uint32_t i = 0; i < q->nslots; i++, s = nxt(q, s)) {
        if (slot_read(q, s, &r) && live(q, &r)) {
            if (q->pending == 0) q->tail = s;
            q->pending++;
        }
    }
    while (q->pending > q->cap) drop_tail(q, true);
}

uint32_t tel_queue_next_seq(const tel_queue_t *q)
{
    return q ? q->next_seq : 0;
}

bool tel_queue_push(tel_queue_t *q, const tel_event_t *e)
{
    if (!q || !e || q->cap == 0 || e->seq < q->next_seq) return false;
    while (q->pending >= q->cap) drop_tail(q, true);

    if (q->head % SPS == 0) {
        /* entrar en un sector: borrarlo. Si aun guarda pendientes (solo tras muchos cortes),
         * se pierden; no hace falta marcarlos, el borrado se los lleva. */
        uint32_t first = q->head, last = q->head + SPS;
        while (q->pending && q->tail >= first && q->tail < last) drop_tail(q, false);
        if (!q->f.erase(q->f.ctx, q->head * TEL_QUEUE_REC, TEL_QUEUE_SECTOR)) return false;
    }

    rec_t r;
    memset(&r, 0xFF, sizeof r);
    r.e = *e;
    r.h.magic = TEL_QUEUE_MAGIC;
    r.h.seq = e->seq;
    r.h.len = sizeof(tel_event_t);
    r.h.ver = REC_VER;
    r.h.crc = rec_crc(&r);

    uint32_t slot = q->head;
    q->head = nxt(q, q->head);  /* aunque falle: un hueco a medias no se reescribe */
    q->next_seq = e->seq + 1;   /* aunque falle: mejor un hueco de seq que repetirlo */
    if (!q->f.write(q->f.ctx, slot * TEL_QUEUE_REC, &r, sizeof r)) return false;
    if (q->pending == 0) q->tail = slot;
    q->pending++;
    return true;
}

int tel_queue_peek(const tel_queue_t *q, tel_event_t *out, int max)
{
    if (!q || !out || max <= 0) return 0;
    rec_t r;
    int n = 0;
    for (uint32_t s = q->tail; s != q->head && n < max && (uint32_t)n < q->pending; s = nxt(q, s)) {
        if (slot_read(q, s, &r) && live(q, &r)) out[n++] = r.e;
    }
    return n;
}

void tel_queue_ack_through(tel_queue_t *q, uint32_t seq)
{
    if (!q) return;
    rec_t r;
    uint32_t s = q->tail;
    while (q->pending && s != q->head) {
        if (slot_read(q, s, &r) && live(q, &r)) {
            if (r.h.seq > seq) break;
            mark_done(q, s);
            q->pending--;
        }
        s = nxt(q, s);
    }
    q->tail = q->pending ? s : q->head;
    if (seq > q->acked) q->acked = seq;
    if (q->acked >= q->next_seq) q->next_seq = q->acked + 1;
}

uint32_t tel_queue_pending(const tel_queue_t *q)
{
    return q ? q->pending : 0;
}

uint32_t tel_queue_lost(const tel_queue_t *q)
{
    return q ? q->lost : 0;
}
