#include <string.h>
#include "pd_render.h"

/* Pattern bytes: a set bit is white. The table below lists black bits, inverted at use. */
#define BLACK_ROWS_D25 { 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 }
#define BLACK_ROWS_D50 { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 }

static LCDPattern g_pat[5];
static int g_pat_ready = 0;
static LCDBitmap** g_images = NULL;
static int g_image_count = 0;

void pd_render_set_images(LCDBitmap** images, int count) {
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

static void exec_bitmap(PlaydateAPI* pd, const RenderCmd* c) {
    LCDBitmap* bmp;
    if (c->image < 0 || c->image >= g_image_count || !g_images) return;
    bmp = g_images[c->image];
    if (!bmp) return;
    if (c->angle_deg == 0.0f && !c->flip) {
        int w = 0, h = 0, rb = 0;
        float left, top;
        pd->graphics->getBitmapData(bmp, &w, &h, &rb, NULL, NULL);
        left = c->x - c->cx * (float)w * c->sx;
        top = c->y - c->cy * (float)h * c->sy;
        pd->graphics->drawScaledBitmap(bmp, iround(left), iround(top), c->sx, c->sy);
    } else {
        pd->graphics->drawRotatedBitmap(bmp, iround(c->x), iround(c->y), c->angle_deg,
                                        c->cx, c->cy, c->flip ? -c->sx : c->sx, c->sy);
    }
}

void pd_render_execute(PlaydateAPI* pd, const RenderList* rl) {
    const struct playdate_graphics* gfx = pd->graphics;
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
