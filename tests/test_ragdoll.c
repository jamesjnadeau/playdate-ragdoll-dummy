#include <string.h>
#include "test.h"
#include "ragdoll.h"
#include "draw_world.h"
#include "game.h"
#include "tuning.h"

static DummyRig g_rigs[DS_COUNT];
static RenderList g_rl;

static void init_rigs(void) {
    for (int i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g_rigs[i]);
}

/* A dummy in the RAGDOLL state at (x, z) and its ragdoll with the given launch. Returns the ragdoll index. */
static int mk_ragdoll(RagdollPool* rp, DummyPool* dp, DummySize s, float x, float z, Vec3 launch) {
    int d = dummy_spawn(dp, s, x, z, -1);
    dp->d[d].state = DST_RAGDOLL;
    return ragdoll_spawn(rp, dp, d, &g_rigs[s], launch);
}

static int mk_live(DummyPool* dp, DummySize s, float x, float z) {
    int d = dummy_spawn(dp, s, x, z, -1);
    dp->d[d].state = DST_WALK;
    return d;
}

static int active_count(const RagdollPool* rp) {
    int n = 0;
    for (int i = 0; i < 6; i++) if (rp->r[i].active) n++;
    return n;
}

static void step(RagdollPool* rp, DummyPool* dp, Hallway* h, RagdollStepOut* out) {
    ragdoll_step(rp, dp, h, g_rigs, out);
}

static Vec3 V(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }

static void test_ragdoll_pool_full(void) {
    static RagdollPool rp; static DummyPool dp;
    int idx[7], d0 = 0;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp);
    for (int i = 0; i < 7; i++) {
        idx[i] = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 2.0f + 0.1f * (float)i, V(0, 0, 0));
        if (i == 0) d0 = rp.r[idx[0]].dummy;
        if (i < 6) CHECK_EQ_INT(active_count(&rp), i + 1);
    }
    CHECK_EQ_INT(active_count(&rp), 6);
    CHECK_EQ_INT(idx[6], idx[0]);                  /* the oldest slot was reused */
    CHECK(!dp.d[d0].alive);                        /* ... and its dummy does not dangle */
    CHECK_EQ_INT(dp.d[d0].state, DST_DEAD);
}

static void test_ragdoll_falls_and_rests(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int r, d, steps = 0;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    r = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 3.0f, V(0, 0, 0));
    d = rp.r[r].dummy;
    for (int i = 0; i < RP_COUNT; i++) { rp.r[r].p[i].y += 0.6f; rp.r[r].prev[i].y += 0.6f; }
    while (rp.r[r].active && steps < 300) {
        step(&rp, &dp, &h, &out);
        steps++;
        for (int i = 0; i < RP_COUNT; i++) CHECK(rp.r[r].p[i].y >= -1e-4f);
    }
    CHECK(!rp.r[r].active);
    CHECK(steps > RAGDOLL_REST_FRAMES);
    CHECK(steps < 300);
    CHECK_EQ_INT(dp.d[d].state, DST_DEAD);
    CHECK(!dp.d[d].alive);
    CHECK_EQ_INT(out.burst_count, 0);              /* it settled, it did not burst */
}

static float stick_err(const Ragdoll* r) {
    static const int ends[9][2] = { {RP_BASE, RP_BODY_LO}, {RP_BODY_LO, RP_BODY_HI}, {RP_BODY_HI, RP_HEAD},
        {RP_BODY_HI, RP_ELBOW_L}, {RP_ELBOW_L, RP_FIST_L}, {RP_BODY_HI, RP_ELBOW_R}, {RP_ELBOW_R, RP_FIST_R},
        {RP_BASE, RP_BODY_HI}, {RP_BODY_LO, RP_HEAD} };
    float worst = 0.0f;
    for (int i = 0; i < 9; i++) {
        float len = v3_len(v3_sub(r->p[ends[i][0]], r->p[ends[i][1]]));
        float e = fabsf(len - r->rest[i]) / r->rest[i];
        if (e > worst) worst = e;
    }
    return worst;
}

static void test_ragdoll_constraints_hold(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int r;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    r = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 3.0f, V(8, 4, 6));
    rp.r[r].hp = 1e9f;
    for (int i = 0; i < 100; i++) {
        step(&rp, &dp, &h, &out);
        if (i == 30 || i == 60) CHECK(stick_err(&rp.r[r]) < 0.15f);
    }
    CHECK(rp.r[r].active);
    CHECK(stick_err(&rp.r[r]) < 0.15f);
}

static void test_wall_impact_damage(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int r, d, bursts = 0;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    r = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.9f, 3.0f, V(4.0f, 0, 0));
    CHECK_NEAR(rp.r[r].hp, RAGDOLL_HP, 1e-6f);
    for (int i = 0; i < 6; i++) step(&rp, &dp, &h, &out);
    CHECK(rp.r[r].hp < RAGDOLL_HP);
    CHECK(rp.r[r].hp > 0.0f);
    CHECK(rp.r[r].active);

    /* A near-dead ragdoll bursts on the wall; the dummy is gone. */
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp);
    r = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.9f, 3.0f, V(6.0f, 0, 0));
    d = rp.r[r].dummy;
    rp.r[r].hp = 0.5f;
    for (int i = 0; i < 10; i++) {
        step(&rp, &dp, &h, &out);
        bursts += out.burst_count;
        if (out.burst_count > 0) {
            CHECK_EQ_INT(out.burst_idx[0], r);
            CHECK(out.burst_at[0].x > 0.5f);
        }
    }
    CHECK_EQ_INT(bursts, 1);
    CHECK(!rp.r[r].active);
    CHECK_EQ_INT(dp.d[d].state, DST_DEAD);
    CHECK(!dp.d[d].alive);
}

static void test_burst_reported_once(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int total = 0;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    mk_ragdoll(&rp, &dp, DS_LARGE, 1.0f, 3.0f, V(12, 3, 0));
    for (int i = 0; i < 80; i++) { step(&rp, &dp, &h, &out); total += out.burst_count; }
    CHECK_EQ_INT(total, 1);
    CHECK_EQ_INT(active_count(&rp), 0);
}

static void test_below_threshold_no_damage(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int r;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    r = mk_ragdoll(&rp, &dp, DS_MEDIUM, 1.0f, 3.0f, V(RAGDOLL_IMPACT_MIN * 0.5f, 0, 0));
    for (int i = 0; i < 12; i++) step(&rp, &dp, &h, &out);
    CHECK_NEAR(rp.r[r].hp, RAGDOLL_HP, 1e-6f);
}

static void test_bowling_damages_dummy(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int b, first = -1, last = -1, total = 0;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 0.0f, V(0, 1, 8));
    b = mk_live(&dp, DS_MEDIUM, 0.0f, 1.5f);
    dp.d[b].hp = 50.0f;
    for (int i = 0; i < 40; i++) {
        step(&rp, &dp, &h, &out);
        if (out.bowl_hits > 0) {
            total += out.bowl_hits;
            if (first < 0) {
                first = i;
                CHECK(dp.d[b].reaction_t > 0);
                CHECK(dp.d[b].reaction == RX_FROM_LEFT || dp.d[b].reaction == RX_FROM_RIGHT);
                CHECK_EQ_INT(dp.d[b].state, DST_STAGGER);
            } else if (i - last < 10) {
                CHECK(0);                          /* same pair hit again inside the cooldown */
            }
            last = i;
        }
    }
    CHECK(first >= 0);
    CHECK(total >= 1);
    CHECK(dp.d[b].hp < 50.0f);
    CHECK(dp.d[b].z > 1.5f);                       /* pushed along the ragdoll's velocity */
}

static void test_bowling_chain_knockout(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int b, r1;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    r1 = mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 0.0f, V(0, 1, 8));
    b = mk_live(&dp, DS_MEDIUM, 0.0f, 1.5f);
    dp.d[b].hp = 0.1f;
    for (int i = 0; i < 20 && dp.d[b].state != DST_RAGDOLL; i++) step(&rp, &dp, &h, &out);
    CHECK_EQ_INT(dp.d[b].state, DST_RAGDOLL);
    CHECK_EQ_INT(active_count(&rp), 2);
    CHECK(dp.d[b].ragdoll >= 0 && dp.d[b].ragdoll != r1);
    CHECK(rp.r[dp.d[b].ragdoll].active);
    CHECK_EQ_INT(rp.r[dp.d[b].ragdoll].dummy, b);
    CHECK(dp.d[b].launch.z > 1.0f);                /* 0.6 x the hitting point's velocity */
}

static void test_ragdoll_slams_open_door(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h; RagdollStepOut out;
    int e;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
    h.ndoors = 1;
    h.doors[0].side = 1; h.doors[0].z = 3.0f; h.doors[0].state = DOOR_OPEN;
    h.doors[0].remaining = 2; h.doors[0].leaf_angle = 1.0f;
    e = dummy_spawn(&dp, DS_MEDIUM, 1.3f, 4.0f, 0);   /* still EMERGING from that door */
    mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.8f, 3.5f, V(12, 0, 0));
    for (int i = 0; i < 8 && h.doors[0].state == DOOR_OPEN; i++) step(&rp, &dp, &h, &out);
    CHECK_EQ_INT(h.doors[0].state, DOOR_CLOSED_FULL);
    CHECK_EQ_INT(h.doors[0].remaining, 2);
    CHECK_NEAR(h.doors[0].leaf_angle, 0.0f, 1e-6f);
    CHECK_NEAR(dp.d[e].hp, dummy_max_hp(DS_MEDIUM) - RAGDOLL_DOORSLAM_DMG, 1e-4f);
    CHECK_EQ_INT(dp.d[e].state, DST_STAGGER);

    /* A slow touch, the other wall, and a hit outside the door's z-span leave it open. */
    {
        Vec3 l[3] = { V(0.5f, 0, 0), V(-12.0f, 0, 0), V(12.0f, 0, 0) };
        float z[3] = { 3.5f, 3.5f, 5.0f };
        for (int k = 0; k < 3; k++) {
            memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp);
            h.doors[0].state = DOOR_OPEN; h.doors[0].leaf_angle = 1.0f;
            mk_ragdoll(&rp, &dp, DS_MEDIUM, k == 0 ? 1.2f : 0.8f, z[k], l[k]);
            for (int i = 0; i < 12; i++) step(&rp, &dp, &h, &out);
            CHECK_EQ_INT(h.doors[0].state, DOOR_OPEN);
        }
    }
}

static void test_game_spawns_and_reports_bursts(void) {
    static Game g;
    GameInput in;
    int i, d, seen = 0;
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    d = dummy_spawn(&g.dummies, DS_MEDIUM, 0.9f, 3.0f, -1);
    g.dummies.d[d].state = DST_RAGDOLL;
    g.dummies.d[d].launch = V(6, 0, 0);
    CHECK_EQ_INT(g.dummies.d[d].ragdoll, -1);
    game_update(&g, &in);
    CHECK(g.dummies.d[d].ragdoll >= 0);
    CHECK(g.ragdolls.r[g.dummies.d[d].ragdoll].active);
    g.ragdolls.r[g.dummies.d[d].ragdoll].hp = 0.2f;
    for (i = 0; i < 20; i++) {
        game_update(&g, &in);
        if (g.nbursts > 0) { seen++; CHECK(g.bursts[0].x > 0.5f); }
    }
    CHECK_EQ_INT(seen, 1);
    CHECK(!g.dummies.d[d].alive);
    game_update(&g, &in);
    CHECK_EQ_INT(g.nbursts, 0);
}

static int find_image(const RenderList* rl, int image) {
    for (int i = 0; i < rl->n; i++) if (rl->cmd[i].type == RC_BITMAP && rl->cmd[i].image == image) return i;
    return -1;
}

static void test_draw_ragdoll_segments(void) {
    static RagdollPool rp; static DummyPool dp;
    Camera cam = {0.0f, 0.0f, 1};
    Ragdoll* r;
    int hi, f, b;
    init_rigs();
    memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp);
    r = &rp.r[mk_ragdoll(&rp, &dp, DS_MEDIUM, 0.0f, 3.0f, V(0, 0, 0))];
    rl_clear(&g_rl);
    draw_ragdoll(&g_rl, &cam, r, &g_rigs[DS_MEDIUM]);
    CHECK(g_rl.n >= 8);
    CHECK_EQ_INT(g_rl.dropped, 0);
    CHECK(find_image(&g_rl, img_dummy(DS_MEDIUM, PART_BASE)) >= 0);
    CHECK(find_image(&g_rl, img_dummy(DS_MEDIUM, PART_BODY)) >= 0);
    CHECK(find_image(&g_rl, img_dummy(DS_MEDIUM, PART_FIST)) >= 0);
    CHECK(find_image(&g_rl, img_dummy(DS_MEDIUM, PART_FACE_KO)) >= 0);
    CHECK(find_image(&g_rl, img_dummy(DS_MEDIUM, PART_FACE_MEAN)) < 0);
    b = find_image(&g_rl, img_dummy(DS_MEDIUM, PART_BODY));
    CHECK(fabsf(g_rl.cmd[b].angle_deg) < 12.0f);      /* standing: nearly upright */

    /* Lay the torso and head out to the right: the body and head turn to 90 degrees. */
    r->p[RP_BODY_LO].y = 0.5f; r->p[RP_BODY_HI].y = 0.5f; r->p[RP_BODY_HI].x = 0.5f;
    r->p[RP_BODY_LO].x = 0.0f; r->p[RP_BASE].x = -0.3f; r->p[RP_BASE].y = 0.5f;
    r->p[RP_HEAD].x = 0.8f; r->p[RP_HEAD].y = 0.5f;
    rl_clear(&g_rl);
    draw_ragdoll(&g_rl, &cam, r, &g_rigs[DS_MEDIUM]);
    b = find_image(&g_rl, img_dummy(DS_MEDIUM, PART_BODY));
    hi = find_image(&g_rl, img_dummy(DS_MEDIUM, PART_HEAD));
    f = find_image(&g_rl, img_dummy(DS_MEDIUM, PART_FACE_KO));
    CHECK(b >= 0 && hi >= 0 && f >= 0);
    CHECK_NEAR(g_rl.cmd[b].angle_deg, 90.0f, 5.0f);
    CHECK_NEAR(g_rl.cmd[hi].angle_deg, 90.0f, 5.0f);
    CHECK_NEAR(g_rl.cmd[f].angle_deg, 90.0f, 5.0f);
}

static void test_draw_world_sorts_ragdolls_with_dummies(void) {
    static RagdollPool rp; static DummyPool dp;
    static Hallway h;
    Camera cam = {0.0f, 0.0f, 1};
    int live, rag;
    init_rigs();
    for (int pass = 0; pass < 2; pass++) {
        memset(&rp, 0, sizeof rp); memset(&dp, 0, sizeof dp); memset(&h, 0, sizeof h);
        mk_ragdoll(&rp, &dp, DS_SMALL, -0.5f, pass == 0 ? 6.0f : 2.0f, V(0, 0, 0));
        live = mk_live(&dp, DS_LARGE, 0.5f, 4.0f);
        (void)live;
        rl_clear(&g_rl);
        draw_world(&g_rl, &cam, &h, &dp, g_rigs, &rp, 0);
        rag = find_image(&g_rl, img_dummy(DS_SMALL, PART_FACE_KO));
        live = find_image(&g_rl, img_dummy(DS_LARGE, PART_FACE_MEAN));
        CHECK(rag >= 0 && live >= 0);
        if (pass == 0) CHECK(rag < live); else CHECK(rag > live);
    }
}

static void test_worst_case_frame_budget(void) {
    static Game g;
    static DummyPool scratch;
    int i;
    game_init(&g, 5);
    g.player.z = 0.0f;
    g.hall.ndoors = MAX_DOORS;
    for (i = 0; i < MAX_DOORS; i++) {
        g.hall.doors[i].side = (i & 1) ? 1 : -1;
        g.hall.doors[i].z = 1.0f + 1.3f * (float)i;
        g.hall.doors[i].state = (i % 2 == 0) ? DOOR_OPEN : DOOR_CLOSED_FULL;
        g.hall.doors[i].leaf_angle = 1.0f;
    }
    for (i = 0; i < MAX_DUMMIES; i++) {
        int d = dummy_spawn(&g.dummies, (DummySize)(i % 3), -1.0f + 0.13f * (float)i, 1.0f + 0.8f * (float)i, -1);
        g.dummies.d[d].state = DST_WINDUP;
    }
    for (i = 0; i < 6; i++) {
        int d = dummy_spawn(&scratch, DS_MEDIUM, -1.0f + 0.4f * (float)i, 1.5f + 1.5f * (float)i, -1);
        ragdoll_spawn(&g.ragdolls, &scratch, d, &g.rigs[DS_MEDIUM], V(0, 0, 0));
    }
    for (i = 0; i < MAX_IMPACTS; i++) {
        g.impacts[i].x = 40.0f + 40.0f * (float)i; g.impacts[i].y = 100.0f;
        g.impacts[i].r = IMPACT_MAX_R; g.impacts[i].t = IMPACT_FRAMES;
    }
    g.swing.phase = SWS_SWEEP; g.swing.sweep01 = 0.5f; g.swing.side = SIDE_LEFT;
    game_render(&g);
    CHECK_EQ_INT(g.rl.dropped, 0);
    CHECK(g.rl.n <= 1024 - 128);
}

void suite_ragdoll(void) {
    test_ragdoll_pool_full();
    test_ragdoll_falls_and_rests();
    test_ragdoll_constraints_hold();
    test_wall_impact_damage();
    test_burst_reported_once();
    test_below_threshold_no_damage();
    test_bowling_damages_dummy();
    test_bowling_chain_knockout();
    test_ragdoll_slams_open_door();
    test_game_spawns_and_reports_bursts();
    test_draw_ragdoll_segments();
    test_draw_world_sorts_ragdolls_with_dummies();
    test_worst_case_frame_budget();
}
