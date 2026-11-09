#include "hand_hold.h"

#include <string.h>

/* Diferencias en uint32_t: correctas aunque el reloj desborde. */
static inline uint32_t since(uint32_t now, uint32_t t) { return now - t; }

static void enter(hand_hold_t *h, hh_state_t s, uint32_t now)
{
    h->state = s;
    h->enter_ms = now;
}

static void reset(hand_hold_t *h, uint32_t now)
{
    h->acc_ms = 0;
    enter(h, HH_IDLE, now);
}

/* Al completar: arma el aviso si se puede publicar y la espera ha vencido. */
static void complete(hand_hold_t *h, uint32_t now)
{
    enter(h, HH_DONE, now);
    if (!h->can_send) return;
    if (h->ever_sent && h->cfg.cooldown_ms > 0 && since(now, h->sent_ms) < h->cfg.cooldown_ms) return;
    h->send_pending = true;
    h->ever_sent = true;
    h->sent_ms = now;
}

static void accumulate(hand_hold_t *h, uint32_t now)
{
    h->acc_ms += since(now, h->last_ms);
    h->last_ms = now;
    if (h->acc_ms >= h->cfg.min_ms) {
        h->acc_ms = h->cfg.min_ms;
        complete(h, now);
    }
}

void hh_init(hand_hold_t *h, const hh_config_t *cfg)
{
    memset(h, 0, sizeof(*h));
    h->cfg = *cfg;
    h->state = HH_IDLE;
}

void hh_button(hand_hold_t *h, uint32_t now)
{
    if (h->state == HH_IDLE) enter(h, HH_READY, now);
}

void hh_touch(hand_hold_t *h, bool down, uint32_t now)
{
    if (down) {
        switch (h->state) {
        case HH_IDLE:
            h->acc_ms = 0;
            /* fallthrough */
        case HH_READY:
        case HH_GRACE:
            h->last_ms = now;
            enter(h, HH_HOLDING, now);
            break;
        case HH_THANKS:
            enter(h, HH_DONE, now); /* sigue acompanando; no hay segundo aviso */
            break;
        default:
            break;
        }
        return;
    }
    switch (h->state) {
    case HH_HOLDING:
        accumulate(h, now);
        if (h->state == HH_HOLDING) enter(h, HH_GRACE, now);
        else enter(h, HH_THANKS, now); /* completo justo al soltar */
        break;
    case HH_DONE:
        enter(h, HH_THANKS, now);
        break;
    default:
        break;
    }
}

void hh_set_available(hand_hold_t *h, bool visible, bool can_send, uint32_t now)
{
    h->can_send = can_send;
    if (!visible && h->state != HH_IDLE) reset(h, now);
}

void hh_tick(hand_hold_t *h, uint32_t now)
{
    switch (h->state) {
    case HH_READY:
        if (since(now, h->enter_ms) >= h->cfg.ready_ms) reset(h, now);
        break;
    case HH_HOLDING:
        accumulate(h, now);
        break;
    case HH_GRACE:
        if (since(now, h->enter_ms) >= h->cfg.grace_ms) reset(h, now);
        break;
    case HH_THANKS:
        if (since(now, h->enter_ms) >= h->cfg.thanks_ms) reset(h, now);
        break;
    default:
        break;
    }
}

uint16_t hh_progress_permille(const hand_hold_t *h, uint32_t now)
{
    if (h->state == HH_DONE || h->state == HH_THANKS) return 1000;
    if (h->cfg.min_ms == 0) return 0;
    uint64_t acc = h->acc_ms;
    if (h->state == HH_HOLDING) acc += since(now, h->last_ms);
    if (acc >= h->cfg.min_ms) return 1000;
    return (uint16_t)(acc * 1000u / h->cfg.min_ms);
}

uint8_t hh_grace_left_s(const hand_hold_t *h, uint32_t now)
{
    if (h->state != HH_GRACE) return 0;
    uint32_t el = since(now, h->enter_ms);
    if (el >= h->cfg.grace_ms) return 0;
    return (uint8_t)((h->cfg.grace_ms - el + 999u) / 1000u);
}

bool hh_take_send(hand_hold_t *h)
{
    bool p = h->send_pending;
    h->send_pending = false;
    return p;
}
