#include "test.h"
#include "draw_world.h"
#include "tuning.h"

static RenderList g_rl;

/* Largest y of the 1 px black horizontal lines: the nearest floor cross line. */
static float nearest_cross_y(const RenderList* rl) {
    float best = -1e9f;
    for (int i = 0; i < rl->n; i++) {
        const RenderCmd* c = &rl->cmd[i];
        if (c->type == RC_LINE && c->pts[4] == 1 && c->pts[1] == c->pts[3] && c->pts[1] > best)
            best = (float)c->pts[1];
    }
    return best;
}

static void test_hallway_emits_geometry(void) {
    Camera cam = {0.0f, 0.0f, 1};
    int found = 0;
    rl_clear(&g_rl);
    draw_hallway(&g_rl, &cam);
    CHECK(g_rl.n > 20);
    CHECK_EQ_INT(g_rl.dropped, 0);
    CHECK_EQ_INT(g_rl.cmd[0].type, RC_POLY);
    CHECK_EQ_INT(g_rl.cmd[0].pattern, PAT_D25);
    for (int i = 0; i < g_rl.n; i++) {
        const RenderCmd* c = &g_rl.cmd[i];
        if (c->type == RC_RECT && c->pattern == PAT_BLACK) {
            float cx = c->x + c->sx * 0.5f, cy = c->y + c->sy * 0.5f;
            if (fabsf(cx - 200.0f) <= 2.0f && fabsf(cy - 108.0f) <= 2.0f) found = 1;
        }
    }
    CHECK(found);
}

static void test_hallway_scrolls(void) {
    Camera cam = {0.0f, 0.0f, 1};
    float y0, y1;
    rl_clear(&g_rl);
    draw_hallway(&g_rl, &cam);
    y0 = nearest_cross_y(&g_rl);
    cam.player_z = 0.5f;
    rl_clear(&g_rl);
    draw_hallway(&g_rl, &cam);
    y1 = nearest_cross_y(&g_rl);
    CHECK(y0 > 108.0f);
    CHECK(y0 != y1);
}

static void test_hallway_clips_near(void) {
    /* Standing inside a panel/cross-line period must not emit non-finite or dropped geometry. */
    Camera cam = {1.4f, 0.0f, 1};
    rl_clear(&g_rl);
    draw_hallway(&g_rl, &cam);
    CHECK(g_rl.n > 20);
    CHECK_EQ_INT(g_rl.dropped, 0);
}

void suite_draw_world(void) {
    test_hallway_emits_geometry();
    test_hallway_scrolls();
    test_hallway_clips_near();
}
