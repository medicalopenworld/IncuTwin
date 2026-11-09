#include "hand_payload.h"

#include <stdio.h>

int hand_payload_build(char *out, size_t out_len, const char *incubator_id, unsigned hold_s)
{
    int n = snprintf(out, out_len, "{\"hand_hold\":1,\"incubator_id\":\"%s\",\"hold_s\":%u}",
                     incubator_id ? incubator_id : "", hold_s);
    return (n < 0 || (size_t)n >= out_len) ? -1 : n;
}
