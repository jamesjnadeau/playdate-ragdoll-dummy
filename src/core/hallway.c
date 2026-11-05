#include <math.h>
#include "hallway.h"
#include "tuning.h"

#define PANEL_PERIOD 3.0f
#define PANEL_HALF_W 0.35f
#define PANEL_HALF_L 0.5f
#define CROSS_PERIOD 1.5f
#define BOARD_STEP 0.4f
#define BOARD_COUNT 7           /* x = -1.2, -0.8 ... 1.2 */
#define BASEBOARD_H 0.25f
#define FAR_INNER_FRAC 0.25f    /* inset of the black core, as a fraction of the far quad */

/* Projects a hallway point. Callers keep z >= player_z + NEAR_Z; the fallback clamp
 * guarantees nothing behind the camera is ever projected. */
static void proj(const Camera* cam, float x, float y, float z, float* sx, float* sy) {
    Vec3 w;
    float ppm;
    w.x = x; w.y = y; w.z = z;
    if (!camera_project(cam, w, sx, sy, &ppm)) {
        w.z = cam->player_z + NEAR_Z;
        camera_project(cam, w, sx, sy, &ppm);
    }
}

/* Quad in the plane x = const spanning y0..y1 and z0..z1. */
static void quad_x(RenderList* rl, const Camera* cam, float x, float y0, float y1,
                   float z0, float z1, Pattern p) {
    float xy[8];
    proj(cam, x, y0, z0, &xy[0], &xy[1]);
    proj(cam, x, y1, z0, &xy[2], &xy[3]);
    proj(cam, x, y1, z1, &xy[4], &xy[5]);
    proj(cam, x, y0, z1, &xy[6], &xy[7]);
    rl_poly(rl, xy, 4, p);
}

/* Quad in the plane y = const spanning x0..x1 and z0..z1. */
static void quad_y(RenderList* rl, const Camera* cam, float y, float x0, float x1,
                   float z0, float z1, Pattern p) {
    float xy[8];
    proj(cam, x0, y, z0, &xy[0], &xy[1]);
    proj(cam, x1, y, z0, &xy[2], &xy[3]);
    proj(cam, x1, y, z1, &xy[4], &xy[5]);
    proj(cam, x0, y, z1, &xy[6], &xy[7]);
    rl_poly(rl, xy, 4, p);
}

/* Line along z at fixed x, y. */
static void line_z(RenderList* rl, const Camera* cam, float x, float y, float z0, float z1,
                   int width) {
    float ax, ay, bx, by;
    proj(cam, x, y, z0, &ax, &ay);
    proj(cam, x, y, z1, &bx, &by);
    rl_line(rl, ax, ay, bx, by, width, PAT_BLACK);
}

void draw_hallway(RenderList* rl, const Camera* cam) {
    const float zn = cam->player_z + NEAR_Z;
    const float zf = cam->player_z + FAR_Z;
    float z, ax, ay, bx, by, cx, cy, dx, dy;
    int i;

    /* Walls (left first), ceiling and floor, all clipped to start at NEAR_Z. */
    quad_x(rl, cam, -HALL_HALF_W, 0.0f, HALL_H, zn, zf, PAT_D25);
    quad_x(rl, cam, HALL_HALF_W, 0.0f, HALL_H, zn, zf, PAT_D25);
    quad_y(rl, cam, HALL_H, -HALL_HALF_W, HALL_HALF_W, zn, zf, PAT_D50);
    quad_y(rl, cam, 0.0f, -HALL_HALF_W, HALL_HALF_W, zn, zf, PAT_WHITE);

    /* Ceiling light panels every 3 m (world-anchored), clipped at the near plane. */
    for (z = ceilf((zn + PANEL_HALF_L) / PANEL_PERIOD) * PANEL_PERIOD - PANEL_PERIOD;
         z - PANEL_HALF_L < zf; z += PANEL_PERIOD) {
        float z0 = z - PANEL_HALF_L, z1 = z + PANEL_HALF_L;
        if (z0 < zn) z0 = zn;
        if (z1 > zf) z1 = zf;
        if (z1 <= z0) continue;
        quad_y(rl, cam, HALL_H, -PANEL_HALF_W, PANEL_HALF_W, z0, z1, PAT_WHITE);
    }

    /* Floor board lines running down the hallway. */
    for (i = 0; i < BOARD_COUNT; i++) {
        float x = -1.2f + (float)i * BOARD_STEP;
        line_z(rl, cam, x, 0.0f, zn, zf, 1);
    }

    /* Floor cross lines on world z multiples of 1.5 so they scroll with the player. */
    for (z = ceilf(zn / CROSS_PERIOD) * CROSS_PERIOD; z <= zf; z += CROSS_PERIOD) {
        proj(cam, -HALL_HALF_W, 0.0f, z, &ax, &ay);
        proj(cam, HALL_HALF_W, 0.0f, z, &bx, &by);
        rl_line(rl, ax, ay, bx, by, 1, PAT_BLACK);
    }

    /* Far darkness: D75 quad at FAR_Z with a black core. */
    proj(cam, -HALL_HALF_W, 0.0f, zf, &ax, &ay);
    proj(cam, HALL_HALF_W, HALL_H, zf, &bx, &by);
    cx = ax; cy = by;   /* top-left */
    dx = bx; dy = ay;   /* bottom-right */
    rl_rect(rl, cx, cy, dx - cx, dy - cy, PAT_D75, false);
    {
        float iw = (dx - cx) * (1.0f - 2.0f * FAR_INNER_FRAC);
        float ih = (dy - cy) * (1.0f - 2.0f * FAR_INNER_FRAC);
        rl_rect(rl, cx + (dx - cx) * FAR_INNER_FRAC, cy + (dy - cy) * FAR_INNER_FRAC,
                iw, ih, PAT_BLACK, false);
    }

    /* Baseboards, then 2 px edge lines where the walls meet floor and ceiling. */
    quad_x(rl, cam, -HALL_HALF_W, 0.0f, BASEBOARD_H, zn, zf, PAT_D75);
    quad_x(rl, cam, HALL_HALF_W, 0.0f, BASEBOARD_H, zn, zf, PAT_D75);
    line_z(rl, cam, -HALL_HALF_W, 0.0f, zn, zf, 2);
    line_z(rl, cam, HALL_HALF_W, 0.0f, zn, zf, 2);
    line_z(rl, cam, -HALL_HALF_W, HALL_H, zn, zf, 2);
    line_z(rl, cam, HALL_HALF_W, HALL_H, zn, zf, 2);
}
