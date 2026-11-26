/* IncuTwin — cola persistente de eventos de telemetria (ver tel_queue.h para el formato).
 *
 * Invariantes:
 *  - Los registros se escriben en orden de hueco y con seq creciente. Una cabecera sana (de
 *    cualquier version) cuenta para saber donde se escribio por ultimo y para next_seq; un
 *    registro es pendiente si ademas es legible (version 1 y CRC del evento bien), no esta marcado
 *    como hecho y su seq > acked. Los pendientes estan entre tail y head.
 *  - Un sector se borra justo cuando head entra en el; a mitad de sector solo se escribe en un
 *    hueco en blanco. Como pending <= cap <= (sectores - 1) * 16, el sector que se borra no tiene
 *    pendientes (salvo huecos rotos acumulados por cortes; entonces sus pendientes van a lost).
 *  - Lo descartado y lo confirmado se marca en flash (palabra `hecho` a 0, sin borrar), asi que
 *    no vuelve tras reiniciar aunque se pierda la NVS (restablecimiento de fabrica).
 *  - No es reentrante (usa un bufer estatico): el que llama serializa el acceso.
 */
#include "tel_queue.h"

#include <stddef.h>
#include <string.h>

#define SPS        (TEL_QUEUE_SECTOR / TEL_QUEUE_REC) /* huecos por sector: 16 */
#define REC_VER    TEL_QUEUE_EV_VER
#define NOT_DONE   0xFFFFFFFFu
#define SCAN_SLOTS 4u                                 /* huecos por lectura al abrir */

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint16_t len;  /* bytes del evento tras la cabecera */
    uint16_t ver;  /* formato del evento */
    uint32_t hcrc; /* de magic, seq, len y ver */
    uint32_t pcrc; /* de los len bytes del evento */
    uint32_t done; /* NOT_DONE = pendiente */
} rec_hdr_t;

#define HDR       ((uint32_t)sizeof(rec_hdr_t))
#define DONE_OFF  ((uint32_t)offsetof(rec_hdr_t, done))
#define LEN_MAX   (TEL_QUEUE_REC - HDR)

_Static_assert(sizeof(rec_hdr_t) == 24, "cabecera de 24 B");
_Static_assert(offsetof(rec_hdr_t, done) == 20, "palabra hecho en el byte 20");
_Static_assert(sizeof(tel_event_t) <= TEL_QUEUE_REC - 24, "el evento cabe en el registro");
_Static_assert(TEL_QUEUE_SECTOR % (SCAN_SLOTS * TEL_QUEUE_REC) == 0, "lecturas dentro de un sector");

enum { SLOT_NONE, SLOT_HDR, SLOT_OK }; /* sin cabecera / solo cabecera / legible */

static uint8_t s_buf[SCAN_SLOTS * TEL_QUEUE_REC]; /* 1 KB: un registro o, al abrir, 4 */

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

/* Clasifica la imagen de un hueco (256 B) y copia su cabecera. */
static int parse(const uint8_t *p, rec_hdr_t *h)
{
    memcpy(h, p, sizeof *h);
    if (h->magic != TEL_QUEUE_MAGIC || h->len > LEN_MAX) return SLOT_NONE;
    if (crc32_upd(0, p, offsetof(rec_hdr_t, hcrc)) != h->hcrc) return SLOT_NONE;
    if (h->ver != REC_VER || crc32_upd(0, p + HDR, h->len) != h->pcrc) return SLOT_HDR;
    return SLOT_OK;
}

/* Evento de un hueco legible: rellena con ceros si es mas corto, recorta si es mas largo. Un `d`
 * recortado sin su NUL seria JSON roto: entonces queda "{}". */
static void decode(const uint8_t *p, const rec_hdr_t *h, tel_event_t *e)
{
    memset(e, 0, sizeof *e);
    memcpy(e, p + HDR, h->len < sizeof *e ? h->len : sizeof *e);
    e->seq = h->seq;
    e->kind[TEL_KIND_MAX - 1] = '\0';
    if (!memchr(e->d, '\0', TEL_D_MAX)) strcpy(e->d, "{}");
}

/* Sube max_gen/max_boot con los de un registro legible, si los trae donde los pone esta version. */
static void note_ids(tel_queue_t *q, const uint8_t *ev, uint16_t len)
{
    if (len < offsetof(tel_event_t, boot) + sizeof(uint32_t)) return;
    if (!memchr(ev + offsetof(tel_event_t, d), '\0', TEL_D_MAX)) return; /* `d` de otro tamano */
    uint32_t g, b;
    memcpy(&g, ev + offsetof(tel_event_t, gen), sizeof g);
    memcpy(&b, ev + offsetof(tel_event_t, boot), sizeof b);
    if (g > q->max_gen) q->max_gen = g;
    if (b > q->max_boot) q->max_boot = b;
}

static uint32_t nxt(const tel_queue_t *q, uint32_t s)
{
    return s + 1 == q->nslots ? 0 : s + 1;
}

/* Lee el hueco en s_buf y lo clasifica. */
static int slot_get(const tel_queue_t *q, uint32_t slot, rec_hdr_t *h)
{
    if (!q->f.read(q->f.ctx, slot * TEL_QUEUE_REC, s_buf, TEL_QUEUE_REC)) return SLOT_NONE;
    return parse(s_buf, h);
}

static bool live(const tel_queue_t *q, const rec_hdr_t *h)
{
    return h->done == NOT_DONE && h->seq > q->acked;
}

/* Hueco legible y pendiente (deja el registro en s_buf). */
static bool slot_pending(const tel_queue_t *q, uint32_t slot, rec_hdr_t *h)
{
    return slot_get(q, slot, h) == SLOT_OK && live(q, h);
}

static bool slot_blank(const tel_queue_t *q, uint32_t slot)
{
    if (!q->f.read(q->f.ctx, slot * TEL_QUEUE_REC, s_buf, TEL_QUEUE_REC)) return false;
    for (uint32_t i = 0; i < TEL_QUEUE_REC; i++)
        if (s_buf[i] != 0xFF) return false;
    return true;
}

/* Marca el registro como hecho (baja bits, no hace falta borrar). Si falla, al reabrir lo cubre
 * acked_seq (confirmados) o vuelve a descartarse (descartados). */
static void mark_done(tel_queue_t *q, uint32_t slot)
{
    const uint32_t zero = 0;
    (void)q->f.write(q->f.ctx, slot * TEL_QUEUE_REC + DONE_OFF, &zero, sizeof zero);
}

/* tail = primer pendiente desde `from`; si se llega a head no queda ninguno. */
static void seek_tail(tel_queue_t *q, uint32_t from)
{
    rec_hdr_t h;
    uint32_t s = from;
    while (q->pending && s != q->head) {
        if (slot_pending(q, s, &h)) break;
        s = nxt(q, s);
    }
    if (s == q->head) q->pending = 0;
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
    uint32_t size = f->size < TEL_QUEUE_BYTES ? f->size : TEL_QUEUE_BYTES; /* aunque pasen la particion entera */
    uint32_t sectors = size / TEL_QUEUE_SECTOR;
    if (sectors < 2) return;
    q->nslots = sectors * SPS;
    q->cap = (sectors - 1) * SPS;
    if (q->cap > TEL_QUEUE_CAP) q->cap = TEL_QUEUE_CAP;

    /* Una pasada, 4 huecos por lectura: la cabecera sana de seq mas alto marca donde se escribio
     * por ultimo; el pendiente de seq mas bajo es el mas viejo (los seq crecen en el anillo). */
    bool any = false;
    uint32_t max_seq = 0, max_slot = 0, min_seq = 0, min_slot = 0;
    for (uint32_t base = 0; base < q->nslots; base += SCAN_SLOTS) {
        if (!f->read(f->ctx, base * TEL_QUEUE_REC, s_buf, sizeof s_buf)) continue;
        for (uint32_t i = 0; i < SCAN_SLOTS; i++) {
            rec_hdr_t h;
            int k = parse(s_buf + i * TEL_QUEUE_REC, &h);
            if (k == SLOT_NONE) continue;
            if (k == SLOT_OK) note_ids(q, s_buf + i * TEL_QUEUE_REC + HDR, h.len);
            if (!any || h.seq > max_seq) {
                any = true;
                max_seq = h.seq;
                max_slot = base + i;
            }
            if (k == SLOT_OK && live(q, &h)) {
                if (q->pending == 0 || h.seq < min_seq) {
                    min_seq = h.seq;
                    min_slot = base + i;
                }
                q->pending++;
            }
        }
    }
    if (max_seq > acked_seq) q->next_seq = max_seq + 1;
    q->head = any ? nxt(q, max_slot) : 0; /* si no esta en blanco, push lo salta */
    q->tail = q->pending ? min_slot : q->head;
    while (q->pending > q->cap) drop_tail(q, true);
}

uint32_t tel_queue_next_seq(const tel_queue_t *q)
{
    return q ? q->next_seq : 0;
}

bool tel_queue_push(tel_queue_t *q, const tel_event_t *e)
{
    if (!q || !e || q->cap == 0 || e->seq < q->next_seq) return false;
    /* Llena: se descarta el mas viejo y suma a lost, aunque vaya en el lote en vuelo; su PUBACK
     * luego no encuentra nada que confirmar y no pasa nada. */
    while (q->pending >= q->cap) drop_tail(q, true);

    /* a mitad de sector solo en un hueco en blanco (uno a medias no se puede reescribir) */
    while (q->head % SPS != 0 && !slot_blank(q, q->head)) q->head = nxt(q, q->head);
    if (q->head % SPS == 0) {
        /* entrar en un sector: borrarlo. Si aun guarda pendientes (solo tras muchos cortes),
         * se pierden; no hace falta marcarlos, el borrado se los lleva. */
        uint32_t first = q->head, last = q->head + SPS;
        while (q->pending && q->tail >= first && q->tail < last) drop_tail(q, false);
        if (!q->f.erase(q->f.ctx, q->head * TEL_QUEUE_REC, TEL_QUEUE_SECTOR)) return false;
    }

    rec_hdr_t h = {
        .magic = TEL_QUEUE_MAGIC, .seq = e->seq, .len = sizeof(tel_event_t), .ver = REC_VER,
        .done = NOT_DONE,
    };
    memset(s_buf, 0xFF, TEL_QUEUE_REC);
    memcpy(s_buf + HDR, e, sizeof *e);
    h.pcrc = crc32_upd(0, s_buf + HDR, h.len);
    memcpy(s_buf, &h, sizeof h);
    h.hcrc = crc32_upd(0, s_buf, offsetof(rec_hdr_t, hcrc));
    memcpy(s_buf, &h, sizeof h);

    uint32_t slot = q->head;
    q->head = nxt(q, q->head);  /* aunque falle: un hueco a medias no se reescribe */
    q->next_seq = e->seq + 1;   /* aunque falle: mejor un hueco de seq que repetirlo */
    if (!q->f.write(q->f.ctx, slot * TEL_QUEUE_REC, s_buf, HDR + h.len)) return false;
    rec_hdr_t back; /* releer: solo cuenta lo que de verdad quedo en la flash */
    if (slot_get(q, slot, &back) != SLOT_OK || back.seq != e->seq) return false;
    if (q->pending == 0) q->tail = slot;
    q->pending++;
    if (e->gen > q->max_gen) q->max_gen = e->gen;
    if (e->boot > q->max_boot) q->max_boot = e->boot;
    return true;
}

int tel_queue_peek(const tel_queue_t *q, tel_event_t *out, int max)
{
    if (!q || !out || max <= 0) return 0;
    rec_hdr_t h;
    int n = 0;
    for (uint32_t s = q->tail; s != q->head && n < max && (uint32_t)n < q->pending; s = nxt(q, s)) {
        if (slot_pending(q, s, &h)) decode(s_buf, &h, &out[n++]);
    }
    return n;
}

void tel_queue_ack_through(tel_queue_t *q, uint32_t seq)
{
    if (!q) return;
    rec_hdr_t h;
    uint32_t s = q->tail;
    while (q->pending && s != q->head) {
        if (slot_pending(q, s, &h)) {
            if (h.seq > seq) break;
            mark_done(q, s);
            q->pending--;
        }
        s = nxt(q, s);
    }
    if (s == q->head) q->pending = 0;
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

void tel_queue_max_ids(const tel_queue_t *q, uint32_t *gen, uint32_t *boot)
{
    if (gen) *gen = q ? q->max_gen : 0;
    if (boot) *boot = q ? q->max_boot : 0;
}
