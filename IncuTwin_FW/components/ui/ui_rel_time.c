#include "ui_rel_time.h"

#include <stdbool.h>
#include <stdio.h>

void ui_rel_time(uint32_t at, uint32_t now, char *out, size_t n, uint8_t lang)
{
    if (!out || n == 0) return;
    bool en = lang == 1;
    if (at == 0) {
        snprintf(out, n, "-");
        return;
    }
    uint32_t d = now > at ? now - at : 0;
    if (d < 60) {
        snprintf(out, n, "%s", en ? "now" : "ahora");
        return;
    }
    uint32_t v;
    const char *u;
    if (d < 3600) {
        v = d / 60;
        u = "min";
    } else if (d < 86400) {
        v = d / 3600;
        u = "h";
    } else {
        v = d / 86400;
        u = "d";
    }
    if (en) {
        snprintf(out, n, "%lu %s ago", (unsigned long)v, u);
    } else {
        snprintf(out, n, "hace %lu %s", (unsigned long)v, u);
    }
}
