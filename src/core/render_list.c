#include <string.h>
#include "render_list.h"

#define RL_MAX_CMDS 512
#define RL_TEXT_BYTES 1024
#define RL_MAX_POLY_PTS 4
#define RL_COORD_LIMIT 30000.0f

/* Round to the nearest int16 and clamp, so off-screen coordinates cannot wrap. */
static int16_t to_i16(float v) {
    if (v > RL_COORD_LIMIT) v = RL_COORD_LIMIT;
    if (v < -RL_COORD_LIMIT) v = -RL_COORD_LIMIT;
    return (int16_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}

/* Returns the next free command slot, or NULL (and counts a drop) when full. */
static RenderCmd* rl_alloc(RenderList* rl, uint8_t type, Pattern p) {
    RenderCmd* c;
    if (rl->n >= RL_MAX_CMDS) {
        rl->dropped++;
        return NULL;
    }
    c = &rl->cmd[rl->n++];
    memset(c, 0, sizeof(*c));
    c->type = type;
    c->pattern = (uint8_t)p;
    c->image = -1;
    return c;
}

void rl_clear(RenderList* rl) {
    rl->n = 0;
    rl->text_n = 0;
    rl->dropped = 0;
    rl->invert = false;
}

void rl_offset(RenderList* rl, int start, int dx, int dy) {
    int i, k;
    if (start < 0) start = 0;
    for (i = start; i < rl->n; i++) {
        RenderCmd* c = &rl->cmd[i];
        switch (c->type) {
        case RC_POLY:
            for (k = 0; k < c->npts; k++) {
                c->pts[k * 2] = (int16_t)(c->pts[k * 2] + dx);
                c->pts[k * 2 + 1] = (int16_t)(c->pts[k * 2 + 1] + dy);
            }
            break;
        case RC_LINE:
            c->pts[0] = (int16_t)(c->pts[0] + dx); c->pts[1] = (int16_t)(c->pts[1] + dy);
            c->pts[2] = (int16_t)(c->pts[2] + dx); c->pts[3] = (int16_t)(c->pts[3] + dy);
            break;
        default:
            c->x += (float)dx;
            c->y += (float)dy;
            break;
        }
    }
}

void rl_poly(RenderList* rl, const float* xy, int npts, Pattern p) {
    RenderCmd* c;
    int i;
    if (npts < 1 || npts > RL_MAX_POLY_PTS) return;
    c = rl_alloc(rl, RC_POLY, p);
    if (!c) return;
    c->npts = (uint8_t)npts;
    for (i = 0; i < npts * 2; i++) c->pts[i] = to_i16(xy[i]);
}

void rl_line(RenderList* rl, float x0, float y0, float x1, float y1, int width, Pattern p) {
    RenderCmd* c = rl_alloc(rl, RC_LINE, p);
    if (!c) return;
    c->pts[0] = to_i16(x0);
    c->pts[1] = to_i16(y0);
    c->pts[2] = to_i16(x1);
    c->pts[3] = to_i16(y1);
    c->pts[4] = (int16_t)width;
}

void rl_rect(RenderList* rl, float x, float y, float w, float h, Pattern fill, bool outline) {
    RenderCmd* c = rl_alloc(rl, RC_RECT, fill);
    if (!c) return;
    c->x = x;
    c->y = y;
    c->sx = w;
    c->sy = h;
    c->pts[0] = outline ? 1 : 0;
}

void rl_bitmap(RenderList* rl, int image, float x, float y, float angle_deg, float cx, float cy, float sx, float sy, bool flip_x) {
    RenderCmd* c = rl_alloc(rl, RC_BITMAP, PAT_WHITE);
    if (!c) return;
    c->image = (int16_t)image;
    c->x = x;
    c->y = y;
    c->angle_deg = angle_deg;
    c->cx = cx;
    c->cy = cy;
    c->sx = sx;
    c->sy = sy;
    c->flip = flip_x ? 1 : 0;
}

void rl_text(RenderList* rl, const char* s, float x, float y) {
    size_t len = strlen(s);
    RenderCmd* c;
    if ((size_t)rl->text_n + len + 1 > RL_TEXT_BYTES) {
        /* Refuse whole strings; a partial string would be wrong on screen. */
        rl->dropped++;
        return;
    }
    c = rl_alloc(rl, RC_TEXT, PAT_BLACK);
    if (!c) return;
    memcpy(rl->text + rl->text_n, s, len + 1);
    c->text_off = (int16_t)rl->text_n;
    c->x = x;
    c->y = y;
    rl->text_n += (int)len + 1;
}
