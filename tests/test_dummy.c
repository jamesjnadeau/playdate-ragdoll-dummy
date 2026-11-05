#include <string.h>
#include "test.h"
#include "dummy.h"
#include "game.h"
#include "tuning.h"

static void test_dummy_pool_full(void) {
    DummyPool p;
    DummyPool snap;
    int i;
    memset(&p, 0, sizeof(p));
    for (i = 0; i < 16; i++) CHECK_EQ_INT(dummy_spawn(&p, DS_MEDIUM, (float)i * 0.1f, 5.0f + (float)i, 0), i);
    snap = p;
    CHECK_EQ_INT(dummy_spawn(&p, DS_SMALL, 0.0f, 1.0f, 0), -1);
    CHECK(memcmp(&snap, &p, sizeof(p)) == 0);
}

static void make_walk(Dummy* d) { d->state = DST_WALK; d->timer = 0; }

static void run(DummyPool* p, int n, float pz, int* total) {
    int i, dmg;
    for (i = 0; i < n; i++) { dummy_update_all(p, pz, 0.0f, &dmg); if (total) *total += dmg; }
}

static void test_dummy_walks_toward_player(void) {
    DummyPool p;
    int i;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_MEDIUM, 0.0f, 5.0f, -1);
    make_walk(&p.d[i]);
    run(&p, 30, 0.0f, NULL);
    CHECK_NEAR(p.d[i].z, 5.0f - 0.75f, 0.02f);
}

static void test_dummy_stops_in_front(void) {
    DummyPool p;
    int i, k, dmg;
    float min_z = PLAYER_RADIUS + DUMMY_RADIUS_MEDIUM - 1e-4f;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_MEDIUM, 0.5f, 3.0f, -1);
    make_walk(&p.d[i]);
    for (k = 0; k < 200; k++) { dummy_update_all(&p, 0.0f, 0.0f, &dmg); CHECK(p.d[i].z >= min_z); }
    CHECK_NEAR(p.d[i].z, PLAYER_RADIUS + DUMMY_RADIUS_MEDIUM, 1e-3f);
}

static void test_dummy_separation(void) {
    DummyPool p;
    int a, b;
    float dx, dz;
    memset(&p, 0, sizeof(p));
    a = dummy_spawn(&p, DS_MEDIUM, 0.0f, 5.0f, -1);
    b = dummy_spawn(&p, DS_MEDIUM, 0.0f, 5.0f, -1);
    make_walk(&p.d[a]); make_walk(&p.d[b]);
    run(&p, 10, 0.0f, NULL);
    dx = p.d[a].x - p.d[b].x;
    dz = p.d[a].z - p.d[b].z;
    CHECK(sqrtf(dx * dx + dz * dz) >= 2.0f * DUMMY_RADIUS_MEDIUM - 1e-3f);
}

static void test_dummy_walls(void) {
    DummyPool p;
    int i;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_LARGE, 1.59f, 5.0f, -1);
    make_walk(&p.d[i]);
    run(&p, 1, 0.0f, NULL);
    CHECK(p.d[i].x <= HALL_HALF_W - DUMMY_RADIUS_LARGE + 1e-4f);
}

static void test_dummy_emerges(void) {
    DummyPool p;
    int i;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_SMALL, 1.0f, 5.0f, 0);
    run(&p, DUMMY_EMERGE_FRAMES - 1, 0.0f, NULL);
    CHECK_EQ_INT(p.d[i].state, DST_EMERGING);
    CHECK_NEAR(p.d[i].z, 5.0f, 1e-6f);
    run(&p, 1, 0.0f, NULL);
    CHECK_EQ_INT(p.d[i].state, DST_WALK);
}

static void test_dummy_punch_damage(void) {
    DummyPool p;
    int i, f, dmg, hits = 0, first = 0;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_LARGE, 0.0f, 0.9f, -1);
    make_walk(&p.d[i]);
    for (f = 1; f <= 50; f++) {
        dummy_update_all(&p, 0.0f, 0.0f, &dmg);
        if (dmg > 0) { hits++; if (!first) first = f; CHECK_EQ_INT(dmg, 2); }
    }
    CHECK_EQ_INT(first, DUMMY_WINDUP_LARGE + 1);
    /* frame 50 starts a second windup, so only one hit in 50 frames, and none for the cooldown */
    CHECK_EQ_INT(hits, 1);
    CHECK_EQ_INT(p.d[i].state, DST_WINDUP);
}

static void test_dummy_cooldown(void) {
    DummyPool p;
    int i, f, dmg, windup_start = 0;
    memset(&p, 0, sizeof(p));
    i = dummy_spawn(&p, DS_SMALL, 0.0f, 0.7f, -1);
    make_walk(&p.d[i]);
    for (f = 1; f <= 200; f++) {
        DummyState before = p.d[i].state;
        dummy_update_all(&p, 0.0f, 0.0f, &dmg);
        if (before == DST_WALK && p.d[i].state == DST_WINDUP) {
            if (windup_start) CHECK_EQ_INT(f - windup_start, DUMMY_WINDUP_SMALL + DUMMY_PUNCH_FRAMES + DUMMY_PUNCH_COOLDOWN + 1);
            windup_start = f;
        }
    }
}

static void test_spawn_pipeline(void) {
    Game g;
    GameInput in;
    int f, n = 0, i;
    memset(&in, 0, sizeof(in));
    game_init(&g, 42);
    in.held = BTN_A;
    for (f = 0; f < 150; f++) game_update(&g, &in);
    for (i = 0; i < MAX_DUMMIES; i++) n += g.dummies.d[i].alive;
    CHECK(n > 0);
    CHECK(g.hall.ndoors > 0);
}

void suite_dummy(void) {
    test_dummy_pool_full();
    test_dummy_walks_toward_player();
    test_dummy_stops_in_front();
    test_dummy_separation();
    test_dummy_walls();
    test_dummy_emerges();
    test_dummy_punch_damage();
    test_dummy_cooldown();
    test_spawn_pipeline();
}
