#include <string.h>
#include "test.h"
#include "pose.h"
#include "draw_world.h"
#include "tuning.h"
#include "game.h"

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
            if (fabsf(cx - 200.0f) <= 2.0f && fabsf(cy - HORIZON_Y) <= 2.0f) found = 1;
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
    CHECK(y0 > HORIZON_Y);
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
    draw_world(&g_rl, &cam, &h, &dp, g_rigs, NULL, 0, NULL, 0);
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
    draw_world(&g_rl, &cam, &h, &dp, g_rigs, NULL, 5, NULL, 0);
    CHECK_EQ_INT(g_rl.dropped, 0);
    CHECK(g_rl.n < 1024);
}

static void test_hallway_clips_near(void) {
    /* Standing inside a panel/cross-line period must not emit non-finite or dropped geometry. */
    Camera cam = {1.4f, 0.0f, 1};
    rl_clear(&g_rl);
    draw_hallway(&g_rl, &cam);
    CHECK(g_rl.n > 20);
    CHECK_EQ_INT(g_rl.dropped, 0);
}

/* ---- weapon sweep, impacts, hurt shake ---- */

static WeaponRig g_wr;

static const RenderCmd* only_bitmap(const RenderList* rl, int* count) {
    const RenderCmd* found = NULL;
    *count = 0;
    for (int i = 0; i < rl->n; i++)
        if (rl->cmd[i].type == RC_BITMAP) { found = &rl->cmd[i]; (*count)++; }
    return found;
}

static const RenderCmd* draw_one(const Weapon* wp) {
    int n;
    const RenderCmd* c;
    rl_clear(&g_rl);
    draw_weapon(&g_rl, wp, &g_wr, IMG_WEAPON_HELD0 + W_BAT);
    c = only_bitmap(&g_rl, &n);
    CHECK_EQ_INT(n, 1);
    return c;
}

static int count_lines(const RenderList* rl) {
    int n = 0;
    for (int i = 0; i < rl->n; i++) n += rl->cmd[i].type == RC_LINE;
    return n;
}

static void test_weapon_upright_when_docked(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    c = draw_one(&wp);
    if (!c) return;
    CHECK_EQ_INT(g_rl.n, 1);     /* no speed lines and no hands */
    CHECK_NEAR(c->x, 200.0f, 0.5f);
    CHECK_NEAR(c->y, 376.0f - 60.0f, 0.5f);
    CHECK_NEAR(c->angle_deg, 0.0f, 0.001f);
    CHECK_NEAR(c->cx, 30.0f / 60.0f, 0.001f);
    CHECK_NEAR(c->cy, 171.0f / 180.0f, 0.001f);
}

static void test_weapon_follows_angle(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    wp.angle = 30.0f;
    c = draw_one(&wp);
    if (!c) return;
    CHECK_NEAR(c->angle_deg, 30.0f, 0.001f);
    CHECK_NEAR(c->x, 200.0f + 60.0f * sinf(30.0f * 3.14159265f / 180.0f), 0.5f);
    CHECK_NEAR(c->y, 376.0f - 60.0f * cosf(30.0f * 3.14159265f / 180.0f), 0.5f);
}

static void test_weapon_angle_wrapped(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    wp.angle = 390.0f;
    c = draw_one(&wp);
    if (c) CHECK_NEAR(c->angle_deg, 30.0f, 0.001f);
}

static void test_pivot_shift_moves_grip(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    wp.shift = -100.0f;
    c = draw_one(&wp);
    if (!c) return;
    CHECK_NEAR(c->x, 100.0f, 0.5f);
    CHECK_NEAR(c->y, 376.0f - 60.0f, 0.5f);
}

static void test_speed_lines_only_when_fast(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    wp.vel = 2.0f;
    draw_one(&wp);
    CHECK_EQ_INT(count_lines(&g_rl), 0);
    wp.vel = 20.0f;               /* turning clockwise: the lines trail on the left, where it came from */
    c = draw_one(&wp);
    CHECK_EQ_INT(count_lines(&g_rl), SPEED_LINES);
    for (int i = 0; c && i < g_rl.n; i++)
        if (g_rl.cmd[i].type == RC_LINE) { CHECK(g_rl.cmd[i].pts[0] < c->x); CHECK(g_rl.cmd[i].pts[2] < c->x); }
    wp.vel = -20.0f;              /* and on the right when turning back */
    c = draw_one(&wp);
    CHECK_EQ_INT(count_lines(&g_rl), SPEED_LINES);
    for (int i = 0; c && i < g_rl.n; i++)
        if (g_rl.cmd[i].type == RC_LINE) { CHECK(g_rl.cmd[i].pts[0] > c->x); CHECK(g_rl.cmd[i].pts[2] > c->x); }
}

/* Upright, the grip is off the bottom edge and the head is well inside the screen. */
static void test_weapon_head_on_screen_upright(void) {
    Weapon wp = {0};
    const RenderCmd* c;
    float tip_y;
    weapon_rig_default(W_BAT, &g_wr);
    g_wr.w = 60; g_wr.h = 180;
    c = draw_one(&wp);
    if (!c) return;
    tip_y = c->y - (g_wr.grip.y - g_wr.tip.y);
    CHECK(tip_y > 0.0f && tip_y < SCREEN_H);
    CHECK_NEAR(tip_y, 146.0f, 3.0f);
    CHECK(c->y >= 230.0f);
}

static void test_impact_star(void) {
    int polys = 0;
    rl_clear(&g_rl);
    draw_impact(&g_rl, 200.0f, 100.0f, 20.0f);
    CHECK(g_rl.n >= 14);
    CHECK_EQ_INT(g_rl.dropped, 0);
    for (int i = 0; i < g_rl.n; i++) {
        const RenderCmd* c = &g_rl.cmd[i];
        CHECK_EQ_INT(c->type, RC_POLY);
        polys++;
    }
    CHECK(polys == g_rl.n);
    CHECK_EQ_INT(g_rl.cmd[0].pattern, PAT_BLACK);              /* 2 px outline first */
    CHECK_EQ_INT(g_rl.cmd[g_rl.n - 1].pattern, PAT_WHITE);     /* white star on top */
}

static void test_rl_offset(void) {
    RenderList* rl = &g_rl;
    float xy[6] = { 10, 10, 20, 10, 15, 20 };
    rl_clear(rl);
    rl_poly(rl, xy, 3, PAT_BLACK);
    rl_line(rl, 0, 0, 5, 5, 1, PAT_BLACK);
    rl_rect(rl, 1, 2, 3, 4, PAT_WHITE, false);
    rl_bitmap(rl, 0, 50, 60, 0, 0.5f, 0.5f, 1, 1, false);
    rl_offset(rl, 0, 3, -2);
    CHECK_EQ_INT(rl->cmd[0].pts[0], 13);
    CHECK_EQ_INT(rl->cmd[0].pts[1], 8);
    CHECK_EQ_INT(rl->cmd[1].pts[2], 8);
    CHECK_EQ_INT(rl->cmd[1].pts[3], 3);
    CHECK_NEAR(rl->cmd[2].x, 4.0f, 0.001f);
    CHECK_NEAR(rl->cmd[3].y, 58.0f, 0.001f);
    rl_offset(rl, 3, 1, 1);      /* only from index 3 */
    CHECK_EQ_INT(rl->cmd[0].pts[0], 13);
    CHECK_NEAR(rl->cmd[3].x, 54.0f, 0.001f);
}

static GameInput no_input(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    return in;
}

static void test_hurt_shake(void) {
    static Game g;
    GameInput in = no_input();
    game_init(&g, 1);
    game_render(&g);
    CHECK(!g.rl.invert);
    g.pending_damage = 1;
    game_update(&g, &in);
    game_render(&g);
    CHECK(g.rl.invert);                       /* the hurt frame inverts */
    for (int f = 0; f < HURT_SHAKE_FRAMES + 2; f++) {
        game_update(&g, &in);
        game_render(&g);
        CHECK(!g.rl.invert);                  /* ... and only that one frame */
    }
}

static void test_hurt_shake_offsets_world_only(void) {
    static Game a, b;
    GameInput in = no_input();
    const RenderCmd *wa = NULL, *wb = NULL;
    game_init(&a, 1); game_init(&b, 1);
    game_update(&a, &in); game_update(&b, &in);
    a.pending_damage = 1;
    game_update(&a, &in); game_update(&b, &in);
    game_update(&a, &in); game_update(&b, &in);    /* a: shaking (not the invert frame), b: calm */
    game_render(&a); game_render(&b);
    CHECK_EQ_INT(a.rl.n, b.rl.n);
    for (int i = 0; i < a.rl.n; i++) {
        const RenderCmd *ca = &a.rl.cmd[i], *cb = &b.rl.cmd[i];
        if (ca->type == RC_BITMAP && ca->image == IMG_WEAPON_HELD0 + W_BAT) { wa = ca; wb = cb; }
    }
    CHECK(wa && wb);
    if (wa && wb) { CHECK_NEAR(wa->x, wb->x, 0.001f); CHECK_NEAR(wa->y, wb->y, 0.001f); }
    /* hallway poly 0 moved by a non-zero offset within +-3 px */
    {
        int dx = a.rl.cmd[0].pts[0] - b.rl.cmd[0].pts[0], dy = a.rl.cmd[0].pts[1] - b.rl.cmd[0].pts[1];
        CHECK(dx >= -HURT_SHAKE_PX && dx <= HURT_SHAKE_PX);
        CHECK(dy >= -HURT_SHAKE_PX && dy <= HURT_SHAKE_PX);
        CHECK(dx != 0 || dy != 0);
    }
}

static int count_polys_white(const RenderList* rl) {
    int n = 0;
    for (int i = 0; i < rl->n; i++) if (rl->cmd[i].type == RC_POLY && rl->cmd[i].pattern == PAT_WHITE) n++;
    return n;
}

/* The weapon is live, clear of the dummies at -70 deg and turning clockwise fast, so this frame's sweep
   crosses the centre. */
static void mid_swing(Game* g) {
    g->wp.live = g->wp.attached = true;
    g->wp.angle = g->wp.prev_angle = -70.0f;
    g->wp.target = 0.0f;
    g->wp.vel = 30.0f;
}

static void test_impacts_spawn_and_expire(void) {
    static Game g;
    GameInput in = no_input();
    int i, before, with, f;
    game_init(&g, 1);
    i = dummy_spawn(&g.dummies, DS_MEDIUM, 0.0f, 1.0f, -1);
    g.dummies.d[i].state = DST_WALK;
    g.dummies.d[i].hp = 100.0f;
    game_render(&g);
    before = count_polys_white(&g.rl);
    mid_swing(&g);
    game_update(&g, &in);
    CHECK_EQ_INT(g.nhits, 1);
    game_render(&g);
    with = count_polys_white(&g.rl);
    CHECK(with >= before + 14);
    in.crank_docked = true;   /* docked from here on: the weapon rests, so no more hits */
    for (f = 0; f < IMPACT_FRAMES - 1; f++) { game_update(&g, &in); game_render(&g); }
    CHECK(count_polys_white(&g.rl) >= before + 14);       /* still shown on the 4th frame */
    game_update(&g, &in); game_render(&g);
    CHECK(count_polys_white(&g.rl) < before + 14);        /* gone on the 5th */
}

/* With all eight stars showing, a new hit recycles the oldest instead of adding a ninth, and a busy frame of
   16 dummies plus the stars drops nothing. (A crank swing loses momentum on each dummy it goes through,
   so it no longer reaches more than eight in one frame.) */
static void test_impacts_capped_at_eight(void) {
    static Game g;
    GameInput in = no_input();
    int k, shown = 0, fresh = 0;
    game_init(&g, 1);
    for (k = 0; k < 16; k++) {
        int i = dummy_spawn(&g.dummies, DS_SMALL, -0.6f + 0.08f * (float)k, 1.0f + 0.01f * (float)k, -1);
        g.dummies.d[i].state = DST_WALK;
        g.dummies.d[i].hp = 100.0f;
    }
    for (k = 0; k < MAX_IMPACTS; k++) {
        g.impacts[k].t = IMPACT_FRAMES - 1 - (k % 3);
        g.impacts[k].x = 40.0f * (float)k + 20.0f; g.impacts[k].y = 60.0f; g.impacts[k].r = 10.0f;
    }
    g.weapon = W_SLEDGE;
    mid_swing(&g);
    game_update(&g, &in);
    CHECK(g.nhits >= 1);
    for (k = 0; k < MAX_IMPACTS; k++) { shown += g.impacts[k].t > 0; fresh += g.impacts[k].t == IMPACT_FRAMES; }
    CHECK_EQ_INT(shown, MAX_IMPACTS);
    CHECK(fresh >= 1 && fresh <= g.nhits);
    game_render(&g);
    CHECK_EQ_INT(g.rl.dropped, 0);
}

static void test_game_weapon_rigs(void) {
    static Game g;
    WeaponRig w[WEAPON_COUNT];
    game_init(&g, 1);
    CHECK(g.wrigs[W_BAT].grip.y > 100.0f);       /* defaults already installed */
    memset(w, 0, sizeof(w));
    w[W_BAT].w = 77;
    game_set_weapon_rigs(&g, w);
    CHECK_EQ_INT(g.wrigs[W_BAT].w, 77);
}


/* ---- Task 18a: floor range patch ---- */

static int index_of_dim25(const RenderList* rl) {
    for (int i = 0; i < rl->n; i++)
        if (rl->cmd[i].type == RC_POLY && rl->cmd[i].pattern == PAT_DIM25) return i;
    return -1;
}

static int dim25_count(const RenderList* rl) {
    int n = 0;
    for (int i = 0; i < rl->n; i++) n += rl->cmd[i].type == RC_POLY && rl->cmd[i].pattern == PAT_DIM25;
    return n;
}

static void test_reach_patch_in_play_only(void) {
    static Game g;
    GameInput in = no_input();
    game_init(&g, 5);
    game_update(&g, &in);
    game_render(&g);
    CHECK_EQ_INT(dim25_count(&g.rl), 1);
    g.mode = MODE_TITLE;
    game_render(&g);
    CHECK_EQ_INT(dim25_count(&g.rl), 0);
    g.mode = MODE_GAME_OVER;
    game_render(&g);
    CHECK_EQ_INT(dim25_count(&g.rl), 0);
}

static void test_reach_patch_drawn_before_dummies(void) {
    static Game g;
    GameInput in = no_input();
    int first_bitmap = -1, i;
    game_init(&g, 5);
    game_update(&g, &in);
    i = dummy_spawn(&g.dummies, DS_MEDIUM, 0.0f, 2.0f, -1);
    g.dummies.d[i].state = DST_WALK;
    game_render(&g);
    for (i = 0; i < g.rl.n; i++) if (g.rl.cmd[i].type == RC_BITMAP) { first_bitmap = i; break; }
    CHECK(first_bitmap > 0);
    CHECK(index_of_dim25(&g.rl) >= 0 && index_of_dim25(&g.rl) < first_bitmap);
}

static float poly_min_x(const RenderCmd* c) {
    float m = 1e9f;
    for (int i = 0; i < c->npts; i++) if ((float)c->pts[2 * i] < m) m = (float)c->pts[2 * i];
    return m;
}

static float poly_max_x(const RenderCmd* c) {
    float m = -1e9f;
    for (int i = 0; i < c->npts; i++) if ((float)c->pts[2 * i] > m) m = (float)c->pts[2 * i];
    return m;
}

static void test_reach_patch_follows_dpad_side(void) {
    static Game g;
    GameInput in = no_input();
    float cx[3] = { 0.0f, 0.0f, 0.0f };
    const uint8_t btn[3] = { BTN_LEFT, 0, BTN_RIGHT };
    game_init(&g, 5);
    for (int k = 0; k < 3; k++) {
        int idx;
        in.held = btn[k];
        game_update(&g, &in);
        game_render(&g);
        idx = index_of_dim25(&g.rl);
        CHECK(idx >= 0);
        if (idx >= 0) cx[k] = 0.5f * (poly_min_x(&g.rl.cmd[idx]) + poly_max_x(&g.rl.cmd[idx]));
    }
    CHECK(cx[0] < cx[1] - 5.0f);
    CHECK(cx[2] > cx[1] + 5.0f);
}

static void test_reach_patch_clamped_to_walls(void) {
    static Game g;
    GameInput in = no_input();
    Vec3 wl = { -HALL_HALF_W, 0.0f, 0.0f }, wr = { HALL_HALF_W, 0.0f, 0.0f };
    float lx, rx, y, ppm;
    int idx;
    game_init(&g, 5);
    in.held = BTN_RIGHT;      /* centre 0.6 plus the arc passes the right wall */
    game_update(&g, &in);
    game_render(&g);
    idx = index_of_dim25(&g.rl);
    CHECK(idx >= 0);
    wl.z = g.cam.player_z + NEAR_Z; wr.z = wl.z;   /* the near edge is the widest */
    CHECK(camera_project(&g.cam, wl, &lx, &y, &ppm));
    CHECK(camera_project(&g.cam, wr, &rx, &y, &ppm));
    if (idx >= 0) {
        CHECK(poly_max_x(&g.rl.cmd[idx]) <= rx + 1.0f);
        CHECK(poly_min_x(&g.rl.cmd[idx]) >= lx - 1.0f);
    }
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
    test_weapon_upright_when_docked();
    test_weapon_follows_angle();
    test_weapon_angle_wrapped();
    test_pivot_shift_moves_grip();
    test_speed_lines_only_when_fast();
    test_weapon_head_on_screen_upright();
    test_impact_star();
    test_rl_offset();
    test_hurt_shake();
    test_hurt_shake_offsets_world_only();
    test_reach_patch_in_play_only();
    test_reach_patch_drawn_before_dummies();
    test_reach_patch_follows_dpad_side();
    test_reach_patch_clamped_to_walls();
    test_impacts_spawn_and_expire();
    test_impacts_capped_at_eight();
    test_game_weapon_rigs();
}
