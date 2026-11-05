#include <math.h>
#include <string.h>
#include "pd_render.h"
#include "rig.h"

/* Pattern bytes: a set bit is white. The table below lists black bits, inverted at use. */
#define BLACK_ROWS_D25 { 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 }
#define BLACK_ROWS_D50 { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 }

static LCDPattern g_pat[5];
static int g_pat_ready = 0;
static LCDBitmap** g_images = NULL;
static int g_image_count = 0;

static void rot_cache_clear(void);

void pd_render_set_images(LCDBitmap** images, int count) {
    rot_cache_clear();
    g_images = images;
    g_image_count = images ? count : 0;
}

static void make_pattern(LCDPattern out, const uint8_t black[8], int invert) {
    for (int i = 0; i < 8; i++) {
        uint8_t b = invert ? (uint8_t)~black[i] : black[i];
        out[i] = (uint8_t)~b;   /* set bit = white */
        out[8 + i] = 0xFF;      /* fully opaque */
    }
}

static void init_patterns(void) {
    static const uint8_t d25[8] = BLACK_ROWS_D25;
    static const uint8_t d50[8] = BLACK_ROWS_D50;
    static const uint8_t none[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t all[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    make_pattern(g_pat[PAT_WHITE], none, 0);
    make_pattern(g_pat[PAT_BLACK], all, 0);
    make_pattern(g_pat[PAT_D25], d25, 0);
    make_pattern(g_pat[PAT_D50], d50, 0);
    make_pattern(g_pat[PAT_D75], d25, 1);   /* inverse of D25 */
    g_pat_ready = 1;
}

static LCDColor color_of(int pattern) {
    if (pattern < 0 || pattern > PAT_D75) pattern = PAT_BLACK;
    return (LCDColor)(uintptr_t)g_pat[pattern];
}

static int iround(float v) {
    return (int)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}

/* ---- rotation cache ----
 * drawRotatedBitmap dominates a busy hallway (every limb is rotated). Rotated images are rendered
 * lazily with drawRotatedBitmap's own result (rotatedBitmap) at ROT_STEPS angle steps and quantised
 * scales (ROT_BUCKETS_PER_OCTAVE per axis), and kept in an LRU, so most rotated draws become a plain
 * drawBitmap. Lookup is a hash chain, so a hit costs a few compares. */
#define ROT_STEPS 32
#define ROT_MAX_PX 320                   /* larger images are drawn directly */
#define ROT_BUDGET_BYTES (1536 * 1024)   /* the cache never holds more than this (1-bit data + mask) */
#define ROT_CACHE 384
#define ROT_HASH 512
#define ROT_BUCKETS_PER_OCTAVE 2
#define ROT_BUCKET_MIN (-5 * ROT_BUCKETS_PER_OCTAVE)   /* scale 1/32 */
#define ROT_BUCKET_MAX (3 * ROT_BUCKETS_PER_OCTAVE)    /* scale 8 */
#define ROT_NB (ROT_BUCKET_MAX - ROT_BUCKET_MIN + 1)
#define DEG_PER_STEP (360.0f / (float)ROT_STEPS)
#define DEG_TO_RAD 0.017453292f

typedef struct {
    LCDBitmap* bmp;
    int image, astep, bx, by, bytes;
    int rw, rh, w, h;          /* rotated and source size in px */
    float sn, cs;              /* sin/cos of the quantised angle */
    int next;                  /* hash chain */
    int lprev, lnext;          /* LRU list, most recent first; free entries are chained through lnext */
} RotEntry;
static RotEntry g_rot[ROT_CACHE];
static int g_head[ROT_HASH];
static bool g_rot_init = false;
static float g_bscale[ROT_NB];       /* scale of each bucket */
static float g_bthresh[ROT_NB];      /* scale above which a value falls in the next bucket */
static int g_lru_head = -1, g_lru_tail = -1, g_free = -1;
static int g_rot_bytes = 0;
static PlaydateAPI* g_pd = NULL;

static unsigned rot_hash(int image, int astep, int bx, int by) {
    return ((unsigned)image * 2654435761u + (unsigned)astep * 40503u + (unsigned)(bx - ROT_BUCKET_MIN) * 977u +
            (unsigned)(by - ROT_BUCKET_MIN) * 131u) >> 7 & (ROT_HASH - 1);
}

static void rot_unlink(int idx) {
    RotEntry* e = &g_rot[idx];
    int* p = &g_head[rot_hash(e->image, e->astep, e->bx, e->by)];
    while (*p >= 0 && *p != idx) p = &g_rot[*p].next;
    if (*p == idx) *p = e->next;
}

static void lru_unlink(int idx) {
    RotEntry* e = &g_rot[idx];
    if (e->lprev >= 0) g_rot[e->lprev].lnext = e->lnext; else g_lru_head = e->lnext;
    if (e->lnext >= 0) g_rot[e->lnext].lprev = e->lprev; else g_lru_tail = e->lprev;
}

static void lru_push_front(int idx) {
    RotEntry* e = &g_rot[idx];
    e->lprev = -1; e->lnext = g_lru_head;
    if (g_lru_head >= 0) g_rot[g_lru_head].lprev = idx; else g_lru_tail = idx;
    g_lru_head = idx;
}

static void rot_free(int idx) {
    RotEntry* e = &g_rot[idx];
    if (!e->bmp) return;
    rot_unlink(idx);
    lru_unlink(idx);
    e->lnext = g_free; g_free = idx;
    if (g_pd) g_pd->graphics->freeBitmap(e->bmp);
    g_rot_bytes -= e->bytes;
    e->bmp = NULL;
}

static void rot_cache_clear(void) {
    if (!g_rot_init) {
        for (int b = 0; b < ROT_NB; b++) {
            g_bscale[b] = exp2f((float)(b + ROT_BUCKET_MIN) / (float)ROT_BUCKETS_PER_OCTAVE);
            g_bthresh[b] = exp2f(((float)(b + ROT_BUCKET_MIN) + 0.5f) / (float)ROT_BUCKETS_PER_OCTAVE);
        }
        g_rot_init = true;
    }
    for (int i = 0; i < ROT_CACHE; i++) rot_free(i);
    for (int i = 0; i < ROT_HASH; i++) g_head[i] = -1;
    g_lru_head = g_lru_tail = -1;
    g_free = -1;
    for (int i = ROT_CACHE - 1; i >= 0; i--) { g_rot[i].bmp = NULL; g_rot[i].lnext = g_free; g_free = i; }
    g_rot_bytes = 0;
}

/* Bucket index (0..ROT_NB-1) of scale s: binary search over the thresholds. */
static int scale_bucket(float s) {
    int lo = 0, hi = ROT_NB - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s > g_bthresh[mid]) lo = mid + 1; else hi = mid;
    }
    return lo;
}

static void draw_direct(PlaydateAPI* pd, const RenderCmd* c, LCDBitmap* src) {
    if (c->angle_deg == 0.0f && !c->flip) {
        int w = 0, h = 0;
        pd->graphics->getBitmapData(src, &w, &h, NULL, NULL, NULL);
        pd->graphics->drawScaledBitmap(src, iround(c->x - c->cx * (float)w * c->sx),
                                       iround(c->y - c->cy * (float)h * c->sy), c->sx, c->sy);
    } else {
        pd->graphics->drawRotatedBitmap(src, iround(c->x), iround(c->y), c->angle_deg,
                                        c->cx, c->cy, c->flip ? -c->sx : c->sx, c->sy);
    }
}

/* Makes the entry for (image, astep, bx, by); returns its index or -1 (draw directly instead). */
static int rot_make(PlaydateAPI* pd, LCDBitmap* src, int image, int astep, int bx, int by) {
    int w = 0, h = 0, rw = 0, rh = 0, sz = 0, i, idx = -1;
    float qa = (float)astep * DEG_PER_STEP;
    LCDBitmap* bmp;
    RotEntry* e;
    pd->graphics->getBitmapData(src, &w, &h, NULL, NULL, NULL);
    if ((float)(w > h ? w : h) * (g_bscale[bx > by ? bx : by]) * 1.42f > (float)ROT_MAX_PX) return -1;
    bmp = pd->graphics->rotatedBitmap(src, qa, g_bscale[bx], g_bscale[by], &sz);
    if (!bmp) return -1;
    /* Over the byte budget, or no free slot: free the least recently used. */
    while (g_lru_tail >= 0 && (g_free < 0 || g_rot_bytes + sz > ROT_BUDGET_BYTES)) rot_free(g_lru_tail);
    idx = g_free;
    if (idx < 0) { pd->graphics->freeBitmap(bmp); return -1; }
    g_free = g_rot[idx].lnext;
    lru_push_front(idx);
    pd->graphics->getBitmapData(bmp, &rw, &rh, NULL, NULL, NULL);
    e = &g_rot[idx];
    e->bmp = bmp; e->image = image; e->astep = astep; e->bx = bx; e->by = by; e->bytes = sz;
    e->rw = rw; e->rh = rh; e->w = w; e->h = h;
    e->sn = sinf(qa * DEG_TO_RAD); e->cs = cosf(qa * DEG_TO_RAD);
    i = (int)rot_hash(image, astep, bx, by);
    e->next = g_head[i]; g_head[i] = idx;
    g_rot_bytes += sz;
    return idx;
}

/* True when the image, wherever it is rotated, cannot reach the screen. Source sizes are fetched once. */
static bool cull_offscreen(PlaydateAPI* pd, const RenderCmd* c, LCDBitmap* src) {
    static short dim[IMG_COUNT];
    float r;
    if (c->image < 0 || c->image >= IMG_COUNT) return false;
    if (dim[c->image] == 0) {
        int w = 0, h = 0;
        pd->graphics->getBitmapData(src, &w, &h, NULL, NULL, NULL);
        dim[c->image] = (short)(w > h ? w : h);
    }
    r = (float)dim[c->image] * (c->sx > c->sy ? c->sx : c->sy) * 1.42f;   /* reach of any pivot-relative corner */
    return c->x + r < 0.0f || c->x - r > 400.0f || c->y + r < 0.0f || c->y - r > 240.0f;
}

/* Draws a rotated image from the cache (see above). */
static void exec_cached(PlaydateAPI* pd, const RenderCmd* c, LCDBitmap* src) {
    float a = c->angle_deg, ox, oy, ccx, ccy;
    int astep, bx, by, idx, dx, dy;
    const RotEntry* e;
    if (c->sx <= 0.0f || c->sy <= 0.0f) { draw_direct(pd, c, src); return; }
    if (cull_offscreen(pd, c, src)) return;
    if (a < 0.0f || a >= 360.0f) a -= 360.0f * (float)(int)floorf(a / 360.0f);   /* floorf is a single FPU op; fmodf is not */
    astep = iround(a * (1.0f / DEG_PER_STEP)) % ROT_STEPS;
    bx = scale_bucket(c->sx);
    by = scale_bucket(c->sy);
    for (idx = g_head[rot_hash(c->image, astep, bx, by)]; idx >= 0; idx = g_rot[idx].next) {
        const RotEntry* t = &g_rot[idx];
        if (t->image == c->image && t->astep == astep && t->bx == bx && t->by == by) break;
    }
    if (idx < 0) idx = rot_make(pd, src, c->image, astep, bx, by);
    if (idx < 0) { draw_direct(pd, c, src); return; }
    if (g_lru_head != idx) { lru_unlink(idx); lru_push_front(idx); }
    e = &g_rot[idx];
    /* The cached image is rotated about its centre; move that centre so the pivot lands on (x, y). */
    ox = (c->cx - 0.5f) * (float)e->w * g_bscale[bx];
    oy = (c->cy - 0.5f) * (float)e->h * g_bscale[by];
    ccx = c->x - (ox * e->cs - oy * e->sn);
    ccy = c->y - (ox * e->sn + oy * e->cs);
    dx = iround(ccx - (float)e->rw * 0.5f);
    dy = iround(ccy - (float)e->rh * 0.5f);
    if (dx >= 400 || dy >= 240 || dx + e->rw <= 0 || dy + e->rh <= 0) return;   /* wholly off screen */
    pd->graphics->drawBitmap(e->bmp, dx, dy, kBitmapUnflipped);
}

static void exec_bitmap(PlaydateAPI* pd, const RenderCmd* c) {
    LCDBitmap* bmp;
    if (c->image < 0 || c->image >= g_image_count || !g_images) return;
    bmp = g_images[c->image];
    if (!bmp) return;
    if (c->flip || c->angle_deg == 0.0f) draw_direct(pd, c, bmp);   /* scaled-only draws are not worth caching */
    else exec_cached(pd, c, bmp);
}

void pd_render_execute(PlaydateAPI* pd, const RenderList* rl) {
    const struct playdate_graphics* gfx = pd->graphics;
    g_pd = pd;
    if (!g_pat_ready) init_patterns();
    pd->display->setInverted(rl->invert ? 1 : 0);
    gfx->clear(kColorWhite);
    for (int i = 0; i < rl->n; i++) {
        const RenderCmd* c = &rl->cmd[i];
        switch (c->type) {
        case RC_POLY: {
            int xy[8];
            int n = c->npts;
            if (n < 1 || n > 4) break;
            for (int k = 0; k < n * 2; k++) xy[k] = c->pts[k];
            gfx->fillPolygon(n, xy, color_of(c->pattern), kPolygonFillNonZero);
            break;
        }
        case RC_LINE:
            gfx->drawLine(c->pts[0], c->pts[1], c->pts[2], c->pts[3], c->pts[4],
                          color_of(c->pattern));
            break;
        case RC_RECT: {
            int x = iround(c->x), y = iround(c->y), w = iround(c->sx), h = iround(c->sy);
            gfx->fillRect(x, y, w, h, color_of(c->pattern));
            if (c->pts[0] == 1) gfx->drawRect(x, y, w, h, kColorBlack);
            break;
        }
        case RC_BITMAP:
            exec_bitmap(pd, c);
            break;
        case RC_TEXT: {
            const char* s = rl->text + c->text_off;
            gfx->drawText(s, strlen(s), kASCIIEncoding, iround(c->x), iround(c->y));
            break;
        }
        default:
            break;
        }
    }
}
