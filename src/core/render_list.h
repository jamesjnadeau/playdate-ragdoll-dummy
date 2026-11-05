#ifndef RENDER_LIST_H
#define RENDER_LIST_H

#define RL_MAX_CMDS 1024
/* The last RL_HUD_RESERVE commands are only available after rl_hud_begin, so the world, particles, impacts
 * and weapon can never crowd out a panel or HUD drawn afterwards. Anything drawn after the world and
 * before rl_hud_begin counts against the capped list. */
#define RL_HUD_RESERVE 128

#include <stdint.h>
#include <stdbool.h>

typedef enum { RC_POLY, RC_LINE, RC_BITMAP, RC_TEXT, RC_RECT } RcType;
typedef enum { PAT_WHITE, PAT_BLACK, PAT_D25, PAT_D50, PAT_D75, PAT_DIM, PAT_DIM25 } Pattern;
/* PAT_DIM: a 50% black checker whose other pixels are transparent, so what is under it shows through.
 * PAT_DIM25 is the same with a 25% black dither (the floor range patch).
 * Every other pattern is opaque. Used for the dim behind panels. */

/*
 * Field use per type:
 *   RC_POLY:   npts, pts[0..2n-1] = xy pairs (1..4 points), pattern = fill
 *   RC_LINE:   pts = x0,y0,x1,y1,width; pattern = colour
 *   RC_RECT:   x, y, sx = width, sy = height; pattern = fill;
 *              pts[0] = 1 for outline, 0 for filled only
 *   RC_BITMAP: image, x, y, angle_deg, cx, cy (normalised centre of rotation),
 *              sx, sy (scale), flip = 1 for flip X
 *   RC_TEXT:   x, y, text_off = offset into RenderList.text
 */
typedef struct {
    uint8_t type, pattern, npts, flip;  /* flip: 0 none, 1 X */
    int16_t image;                      /* ImageId, -1 none */
    int16_t pts[8];                     /* poly: up to 4 xy pairs; line: x0,y0,x1,y1,width */
    float x, y, angle_deg, cx, cy, sx, sy;  /* bitmap: center-of-rotation normalized cx,cy */
    int16_t text_off;                   /* offset into RenderList.text */
} RenderCmd;

typedef struct {
    RenderCmd cmd[RL_MAX_CMDS];
    int n;
    char text[1024];
    int text_n;
    bool invert;    /* the platform inverts the display for this frame */
    bool hud_open;  /* rl_hud_begin was called: the HUD reserve is available */
    int dropped;    /* commands refused because the list or text buffer was full */
} RenderList;

void rl_clear(RenderList* rl);
/* Opens the reserved headroom; call once, before drawing overlays and HUD. rl_clear closes it again. */
void rl_hud_begin(RenderList* rl);
void rl_poly(RenderList* rl, const float* xy, int npts, Pattern p);
void rl_line(RenderList* rl, float x0, float y0, float x1, float y1, int width, Pattern p);
void rl_rect(RenderList* rl, float x, float y, float w, float h, Pattern fill, bool outline);
void rl_bitmap(RenderList* rl, int image, float x, float y, float angle_deg, float cx, float cy, float sx, float sy, bool flip_x);
/* Shifts every command from index start on by (dx, dy) pixels. */
void rl_offset(RenderList* rl, int start, int dx, int dy);
void rl_text(RenderList* rl, const char* s, float x, float y);

#endif
