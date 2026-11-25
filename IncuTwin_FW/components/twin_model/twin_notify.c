#include "twin_notify.h"

static bool inside(twin_baby_t baby)
{
    return baby == TWIN_BABY_IN || baby == TWIN_BABY_PARENTS;
}

void twin_notice_base_boot(twin_notice_base_t *b, bool has_baby, twin_baby_t baby)
{
    b->valid = has_baby;
    b->treatments_known = false;
    b->baby = baby;
    b->heat = false;
    b->photo = false;
}

uint32_t twin_notices_update(twin_notice_base_t *b, const twin_incubator_t *now)
{
    if (!now->online) {
        return 0; /* sin datos fiables: ni se avisa ni se toca la base */
    }
    const bool heat = now->thermo != TWIN_THERMO_OFF;

    if (!b->valid) {
        b->baby = now->baby;
        b->heat = heat;
        b->photo = now->photo;
        b->valid = b->treatments_known = true;
        return 0;
    }

    uint32_t n = 0;
    if (!inside(b->baby) && inside(now->baby)) {
        n |= TWIN_NOTICE_ADMISSION;
    } else if (b->baby == TWIN_BABY_IN && now->baby == TWIN_BABY_PARENTS) {
        n |= TWIN_NOTICE_PARENTS;
    } else if (inside(b->baby) && !inside(now->baby)) {
        n |= TWIN_NOTICE_EXIT;
    }
    if (b->treatments_known) {
        if (heat != b->heat) {
            n |= heat ? TWIN_NOTICE_HEAT_ON : TWIN_NOTICE_HEAT_OFF;
        }
        if (now->photo != b->photo) {
            n |= now->photo ? TWIN_NOTICE_PHOTO_ON : TWIN_NOTICE_PHOTO_OFF;
        }
    } else {
        b->treatments_known = true;
    }
    b->baby = now->baby;
    b->heat = heat;
    b->photo = now->photo;
    return n;
}
