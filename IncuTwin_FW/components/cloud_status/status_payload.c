#include "status_payload.h"

#include <stdio.h>
#include <string.h>

int status_payload_build(char *out, size_t out_len, const char *fw, int rssi,
                         const char *incubator_id, bool acc, int64_t ts)
{
    int n = snprintf(out, out_len, "{\"online\":true,\"fw\":\"%s\",\"rssi\":%d,\"incubator_id\":\"%s\"%s,\"ts\":%lld}",
                     fw ? fw : "", rssi, incubator_id ? incubator_id : "", acc ? ",\"acc\":1" : "",
                     (long long)ts);
    if (n < 0 || (size_t)n >= out_len || n > STATUS_PAYLOAD_MAX) return -1;
    return n;
}

int status_payload_build_named(char *out, size_t out_len, const char *fw, int rssi,
                               const char *incubator_id, bool acc, int64_t ts, const char *name)
{
    int n = status_payload_build(out, out_len, fw, rssi, incubator_id, acc, ts);
    if (n < 0 || !name || !name[0]) return n;
    char clean[64];
    size_t k = 0;
    for (const char *p = name; *p && k < sizeof(clean) - 1; p++) {
        if (*p == '"' || *p == '\\' || (unsigned char)*p < 0x20) continue;
        clean[k++] = *p;
    }
    clean[k] = 0;
    /* quitar la llave final y anadir el nombre */
    int m = snprintf(out + n - 1, out_len - (size_t)(n - 1), ",\"name\":\"%s\"}", clean);
    if (m < 0 || (size_t)(n - 1 + m) >= out_len || n - 1 + m > STATUS_PAYLOAD_MAX) return -1;
    return n - 1 + m;
}
