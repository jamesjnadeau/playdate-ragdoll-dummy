#include <string.h>
#include "test.h"
#include "pose.h"
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

static DummyRig g_rigs[DS_COUNT];

static void init_rigs(void) {
    for (int s = 0; s < DS_COUNT; s++) rig_default((DummySize)s, &g_rigs[s]);
}

/* sy of the first body bitmap of the given size, or -1. */
static float body_sy(const RenderList* rl, DummySize s) {
    for (int i = 0; i < rl->n; i++)
        if (rl->cmd[i].type == RC_BITMAP && rl->cmd[i].image == img_dummy(s, PART_BODY))
            return rl->cmd[i].sy;
    return -1.0f;
}

static void test_draw_dummy_scales_with_distance(void) {
    Camera cam = {0.0f, 0.0f, 1};
    Dummy d;
    DummyPool dp;
    float near_sy, far_sy;
    init_rigs();
    memset(&dp, 0, sizeof(dp));
    dummy_spawn(&dp, DS_MEDIUM, 0.0f, 2.0f, -1);
    dp.d[0].state = DST_DEAD;
    d = dp.d[0];
    d.state = DST_WALK;
    DummyPose pose;
    pose_for_state(&d, 0, &pose);
    pose.sx = pose.sy = 1.0f;
    rl_clear(&g_rl);
    draw_dummy(&g_rl, &cam, &d, &g_rigs[DS_MEDIUM], &pose);
    near_sy = body_sy(&g_rl, DS_MEDIUM);
    d.z = 4.0f;
    rl_clear(&g_rl);
    draw_dummy(&g_rl, &cam, &d, &g_rigs[DS_MEDIUM], &pose);
    far_sy = body_sy(&g_rl, DS_MEDIUM);
    CHECK(far_sy > 0.0f);
    CHECK(near_sy == 2.0f * far_sy);
}

static void test_dummies_sorted_far_to_near(void) {
    Camera cam = {0.0f, 0.0f, 1};
    Hallway h;
    DummyPool dp;
    int first_body = -1;
    init_rigs();
    memset(&h, 0, sizeof(h));
    memset(&dp, 0, sizeof(dp));
    dummy_spawn(&dp, DS_SMALL, 0.3f, 2.0f, -1);    /* near, spawned first */
    dummy_spawn(&dp, DS_LARGE, -0.3f, 6.0f, -1);   /* far */
    dp.d[0].state = DST_WALK; dp.d[1].state = DST_WALK;
    rl_clear(&g_rl);
    draw_world(&g_rl, &cam, &h, &dp, g_rigs, 0);
    for (int i = 0; i < g_rl.n && first_body < 0; i++) {
        int im = g_rl.cmd[i].image;
        if (g_rl.cmd[i].type != RC_BITMAP) continue;
        if (im == img_dummy(DS_SMALL, PART_BODY) || im == img_dummy(DS_LARGE, PART_BODY)) first_body = im;
    }
    CHECK_EQ_INT(first_body, img_dummy(DS_LARGE, PART_BODY));
    CHECK_EQ_INT(g_rl.dropped, 0);
}

static void test_doors_draw(void) {
    Camera cam = {0.0f, 0.0f, 1};
    Door d = { 1, 5.0f, DOOR_CLOSED_FULL, 2, 0, 0.0f };
    int n_closed, n_open;
    rl_clear(&g_rl);
    draw_door(&g_rl, &cam, &d);
    n_closed = g_rl.n;
    CHECK(n_closed >= 3);
    CHECK_EQ_INT(g_rl.cmd[0].pattern, PAT_BLACK);
    d.state = DOOR_OPEN; d.leaf_angle = 1.0f;
    rl_clear(&g_rl);
    draw_door(&g_rl, &cam, &d);
    n_open = g_rl.n;
    CHECK(n_open >= 2);
    CHECK_EQ_INT(g_rl.cmd[0].pattern, PAT_BLACK);
    CHECK_EQ_INT(g_rl.cmd[1].pattern, PAT_WHITE);
    CHECK_EQ_INT(g_rl.dropped, 0);
}

static void test_door_leaf_swings_into_hallway(void) {
    float xr, zr, xl, zl;
    for (int side = -1; side <= 1; side += 2) {
        for (float a = 0.0f; a <= 1.0f; a += 0.25f) {
            Door d = { side, 5.0f, DOOR_OPEN, 1, 0, a };
            float x, z;
            door_leaf_free_edge(&d, &x, &z);
            CHECK(fabsf(x) <= HALL_HALF_W + 1e-5f);
            CHECK(z <= d.z + DOOR_LEN + 1e-4f);
            if (a >= 0.5f) {
                CHECK(fabsf(x) < HALL_HALF_W);
                CHECK(x * (float)side > 0.0f);       /* never through the wall */
            }
        }
        {
            Door d = { side, 5.0f, DOOR_OPEN, 1, 0, 90.0f / DOOR_OPEN_MAX_DEG };
            float x, z;
            door_leaf_free_edge(&d, &x, &z);
            CHECK_NEAR(z, d.z, 1e-4f);               /* 90 deg: perpendicular to the wall */
            CHECK(fabsf(x) < HALL_HALF_W);
            d.leaf_angle = 1.0f;
            door_leaf_free_edge(&d, &x, &z);
            CHECK(fabsf(x) < HALL_HALF_W);
            CHECK(z < d.z);                          /* toward the player */
            CHECK(fabsf(x) > HALL_HALF_W - 0.35f);   /* close to the wall */
        }
    }
    {
        Door r = { 1, 5.0f, DOOR_OPEN, 1, 0, 0.6f }, l = { -1, 5.0f, DOOR_OPEN, 1, 0, 0.6f };
        door_leaf_free_edge(&r, &xr, &zr);
        door_leaf_free_edge(&l, &xl, &zl);
        CHECK_NEAR(xl, -xr, 1e-6f);
        CHECK_NEAR(zl, zr, 1e-6f);
    }
}

static void test_worst_case_no_drops(void) {
    Camera cam = {0.0f, 0.0f, 1};
    Hallway h;
    DummyPool dp;
    init_rigs();
    memset(&h, 0, sizeof(h));
    memset(&dp, 0, sizeof(dp));
    h.ndoors = MAX_DOORS;
    for (int i = 0; i < MAX_DOORS; i++) {
        h.doors[i].side = (i & 1) ? 1 : -1;
        h.doors[i].z = 1.0f + 1.4f * (float)i;
        h.doors[i].state = (i % 3 == 0) ? DOOR_OPEN : DOOR_CLOSED_FULL;
        h.doors[i].leaf_angle = 1.0f;
    }
    for (int i = 0; i < MAX_DUMMIES; i++) {
        dummy_spawn(&dp, (DummySize)(i % 3), -0.9f + 0.12f * (float)i, 1.0f + 0.9f * (float)i, -1);
        dp.d[i].state = DST_WINDUP;
    }
    rl_clear(&g_rl);
    draw_world(&g_rl, &cam, &h, &dp, g_rigs, 5);
    CHECK_EQ_INT(g_rl.dropped, 0);
    CHECK(g_rl.n < 512);
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
    test_draw_dummy_scales_with_distance();
    test_dummies_sorted_far_to_near();
    test_doors_draw();
    test_worst_case_no_drops();
    test_door_leaf_swings_into_hallway();
}
