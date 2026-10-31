#include "status_payload.h"

#include <stdio.h>

int status_payload_build(char *out, size_t out_len, const char *fw, int rssi,
                         const char *incubator_id, int64_t ts)
{
    int n = snprintf(out, out_len, "{\"online\":true,\"fw\":\"%s\",\"rssi\":%d,\"incubator_id\":\"%s\",\"ts\":%lld}",
                     fw ? fw : "", rssi, incubator_id ? incubator_id : "", (long long)ts);
    if (n < 0 || (size_t)n >= out_len || n > STATUS_PAYLOAD_MAX) return -1;
    return n;
}
