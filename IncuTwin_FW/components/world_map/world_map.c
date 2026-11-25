#include "world_map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "map_land.h"

#define SS    4   /* sub-filas por pixel (suavizado vertical) */
#define MAX_X 512 /* cruces por sub-fila */
#define MAX_W 320

void wm_fit(wm_view_t *v, const wm_frame_t *f, float lat1, float lon1, float lat2, float lon2)
{
    float mid_lat = (lat1 + lat2) / 2.0f;
    float k = cosf(mid_lat * (float)M_PI / 180.0f);
    if (k < 0.3f) k = 0.3f;
    float dlon = fabsf(lon1 - lon2), dlat = fabsf(lat1 - lat2);
    float avail_w = (float)(f->w - 2 * f->margin);
    float avail_h = (float)(f->h - f->top - f->bottom - 2 * f->margin);
    float s = f->max_ppd;
    if (dlon > 0.0f && avail_w / (dlon * k) < s) s = avail_w / (dlon * k);
    if (dlat > 0.0f && avail_h / dlat < s) s = avail_h / dlat;
    if (s < f->min_ppd) s = f->min_ppd;
    v->sy = s;
    v->sx = s * k;
    float cx = f->w / 2.0f;
    float cy = f->top + (f->h - f->top - f->bottom) / 2.0f;
    v->lon0 = (lon1 + lon2) / 2.0f - cx / v->sx;
    v->lat0 = mid_lat + cy / v->sy;
}

void wm_project(const wm_view_t *v, float lat, float lon, float *x, float *y)
{
    *x = (lon - v->lon0) * v->sx;
    *y = (v->lat0 - lat) * v->sy;
}

/* Arista ya proyectada a pixeles, con y0 < y1 */
typedef struct {
    float y0, y1, x0, dxdy;
} edge_t;

static int edge_cmp(const void *a, const void *b)
{
    float d = ((const edge_t *)a)->y0 - ((const edge_t *)b)->y0;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

static void sort_f(float *a, int n)
{
    for (int i = 1; i < n; i++) {
        float t = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > t) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = t;
    }
}

static uint16_t mix565(uint16_t a, uint16_t b, float t) /* t = peso de b (0..1) */
{
    int r = ((a >> 11) & 31) + (int)((((b >> 11) & 31) - ((a >> 11) & 31)) * t + 0.5f);
    int g = ((a >> 5) & 63) + (int)((((b >> 5) & 63) - ((a >> 5) & 63)) * t + 0.5f);
    int bl = (a & 31) + (int)(((b & 31) - (a & 31)) * t + 0.5f);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

/* Aristas de todos los anillos que tocan las filas 0..h, proyectadas y ordenadas por y0. */
static int build_edges(edge_t *e, const wm_view_t *v, int h)
{
    int n = 0;
    uint32_t base = 0;
    for (uint16_t r = 0; r < MAP_LAND_RINGS; r++) {
        uint16_t len = map_land_ring_len[r];
        float ytop = (v->lat0 - map_land_ring_lat[r][1] / 100.0f) * v->sy;
        float ybot = (v->lat0 - map_land_ring_lat[r][0] / 100.0f) * v->sy;
        if (ybot >= 0.0f && ytop <= (float)h) {
            for (uint16_t i = 0; i < len; i++) {
                const int16_t *a = map_land_pts[base + i];
                const int16_t *b = map_land_pts[base + (i + 1 == len ? 0 : i + 1)];
                if (a[1] == b[1]) continue; /* horizontal: no cruza ninguna sub-fila */
                float ax = (a[0] / 100.0f - v->lon0) * v->sx, ay = (v->lat0 - a[1] / 100.0f) * v->sy;
                float bx = (b[0] / 100.0f - v->lon0) * v->sx, by = (v->lat0 - b[1] / 100.0f) * v->sy;
                if (ay > by) {
                    float t = ax; ax = bx; bx = t;
                    t = ay; ay = by; by = t;
                }
                if (by < 0.0f || ay > (float)h) continue;
                e[n++] = (edge_t){ ay, by, ax, (bx - ax) / (by - ay) };
            }
        }
        base += len;
    }
    qsort(e, (size_t)n, sizeof(edge_t), edge_cmp);
    return n;
}

/* Relleno par-impar de todos los anillos a la vez (los huecos salen solos) con lista de aristas
 * activas, SS sub-filas por fila y cobertura horizontal exacta de cada tramo. */
void wm_render(uint16_t *buf, int w, int h, const wm_view_t *v, uint16_t land, uint16_t sea)
{
    static float xs[MAX_X];
    static float cov[MAX_W];
    if (w > MAX_W) w = MAX_W;
    edge_t *e = malloc(sizeof(edge_t) * MAP_LAND_POINTS);
    int *act = malloc(sizeof(int) * MAP_LAND_POINTS);
    if (!e || !act) {
        free(e);
        free(act);
        for (int i = 0; i < w * h; i++) buf[i] = sea;
        return;
    }
    int ne = build_edges(e, v, h), next = 0, na = 0;
    for (int y = 0; y < h; y++) {
        memset(cov, 0, sizeof(float) * (size_t)w);
        for (int k = 0; k < SS; k++) {
            float ys = y + (k + 0.5f) / SS;
            while (next < ne && e[next].y0 <= ys) act[na++] = next++;
            int n = 0;
            for (int i = 0; i < na;) {
                const edge_t *ed = &e[act[i]];
                if (ed->y1 <= ys) { /* ya ha pasado: fuera de la lista */
                    act[i] = act[--na];
                    continue;
                }
                if (n < MAX_X) xs[n++] = ed->x0 + (ys - ed->y0) * ed->dxdy;
                i++;
            }
            sort_f(xs, n);
            for (int i = 0; i + 1 < n; i += 2) {
                float x0 = xs[i] < 0 ? 0 : xs[i], x1 = xs[i + 1] > w ? (float)w : xs[i + 1];
                if (x1 <= x0) continue;
                for (int px = (int)x0; px < w && px < x1; px++) {
                    float a = x0 > px ? x0 : (float)px;
                    float b = x1 < px + 1 ? x1 : (float)(px + 1);
                    if (b > a) cov[px] += (b - a) / SS;
                }
            }
        }
        uint16_t *row = buf + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            float c = cov[x] > 1.0f ? 1.0f : cov[x];
            row[x] = c <= 0.0f ? sea : c >= 1.0f ? land : mix565(sea, land, c);
        }
    }
    free(e);
    free(act);
}
