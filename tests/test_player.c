#include <string.h>
#include "test.h"
#include "player.h"
#include "game.h"
#include "tuning.h"

static GameInput held(int buttons) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    in.held = (uint8_t)buttons;
    return in;
}

static void test_player_walks(void) {
    Player p; Hallway h; DummyPool dp; GameInput in = held(BTN_A);
    int f;
    memset(&h, 0, sizeof(h)); memset(&dp, 0, sizeof(dp));
    player_init(&p);
    for (f = 0; f < 30; f++) player_update(&p, &in, &h, &dp, 0);
    CHECK_NEAR(p.z, PLAYER_SPEED, 0.01f);
}

static void test_player_blocked_by_dummy(void) {
    Player p; Hallway h; DummyPool dp; GameInput in = held(BTN_A);
    int f, i;
    memset(&h, 0, sizeof(h)); memset(&dp, 0, sizeof(dp));
    player_init(&p);
    i = dummy_spawn(&dp, DS_MEDIUM, 0.0f, 2.0f, -1);
    dp.d[i].state = DST_WALK;
    for (f = 0; f < 90; f++) player_update(&p, &in, &h, &dp, 0);
    CHECK_NEAR(p.z, 2.0f - 0.38f - 0.35f, 0.01f);
    /* a dummy well off to the side does not block */
    dp.d[i].x = 1.2f;
    for (f = 0; f < 30; f++) player_update(&p, &in, &h, &dp, 0);
    CHECK(p.z > 1.5f);
}

static void setup_door(Hallway* h, int remaining) {
    memset(h, 0, sizeof(*h));
    h->ndoors = 1;
    h->doors[0].side = 1;
    h->doors[0].z = 4.0f;
    h->doors[0].state = DOOR_OPEN;
    h->doors[0].remaining = remaining;
    h->doors[0].spawn_timer = DOOR_SPAWN_INTERVAL;
    h->doors[0].leaf_angle = 1.0f;
    h->next_z = 100.0f;
}

static void test_door_slam(void) {
    Player p; Hallway h; DummyPool dp; GameInput in = held(BTN_A);
    int f, ev = 0, slams = 0;
    float slam_z = 0.0f;
    memset(&dp, 0, sizeof(dp));
    setup_door(&h, 2);
    player_init(&p);
    p.z = 4.4f;
    for (f = 0; f < 10 && !slams; f++) {
        ev = player_update(&p, &in, &h, &dp, 0);
        if (ev & PEV_DOOR_SLAM) { slams++; slam_z = p.z; }
    }
    CHECK_EQ_INT(slams, 1);
    CHECK(slam_z >= 4.55f - 0.01f);
    CHECK_EQ_INT(h.doors[0].spawn_timer, 0);
    CHECK_EQ_INT(h.doors[0].state, DOOR_OPEN);
    CHECK_EQ_INT(p.hearts, PLAYER_HEARTS);
    for (f = 0; f < KNOCKBACK_FRAMES; f++) player_update(&p, &in, &h, &dp, 0);
    CHECK_NEAR(p.z, slam_z - KNOCKBACK_DIST, 0.05f);
}

static void test_door_slam_spawns_dummy(void) {
    Game g; GameInput in = held(BTN_A);
    int f, i, found = -1;
    game_init(&g, 7);
    setup_door(&g.hall, 2);
    g.player.z = 4.5f;
    g.cam.player_z = 4.5f;
    for (f = 0; f < 3; f++) game_update(&g, &in);
    for (f = 0; f < KNOCKBACK_FRAMES; f++) game_update(&g, &in);
    CHECK(g.player.z < 4.55f - 0.5f);
    CHECK_NEAR(g.cam.player_z, g.player.z, 1e-6f);
    for (i = 0; i < MAX_DUMMIES; i++)
        if (g.dummies.d[i].alive && g.dummies.d[i].door == 0) found = i;
    CHECK(found >= 0);
    if (found >= 0) CHECK_EQ_INT(g.dummies.d[found].state, DST_EMERGING);
    CHECK_EQ_INT(g.hall.doors[0].remaining, 1);
}

static void test_door_closes_when_empty(void) {
    Player p; Hallway h; DummyPool dp; GameInput in = held(BTN_A);
    int f, ev, closed = 0;
    memset(&dp, 0, sizeof(dp));
    setup_door(&h, 0);
    player_init(&p);
    p.z = 4.4f;
    for (f = 0; f < 10; f++) {
        ev = player_update(&p, &in, &h, &dp, 0);
        if (ev & PEV_DOOR_CLOSED) closed++;
        CHECK(!(ev & PEV_DOOR_SLAM));
    }
    CHECK_EQ_INT(closed, 1);
    CHECK_EQ_INT(h.doors[0].state, DOOR_CLOSED_DONE);
    CHECK_EQ_INT(h.doors_emptied, 1);
}

static void test_cant_walk_back_past_closed_door(void) {
    Player p; Hallway h; DummyPool dp; GameInput fwd = held(BTN_A), back = held(BTN_B);
    int f;
    memset(&dp, 0, sizeof(dp));
    setup_door(&h, 0);
    player_init(&p);
    p.z = 4.4f;
    for (f = 0; f < 10; f++) player_update(&p, &fwd, &h, &dp, 0);
    CHECK(p.z > 4.55f);
    for (f = 0; f < 120; f++) { player_update(&p, &back, &h, &dp, 0); CHECK(p.z >= 4.0f - 1e-4f); }
    CHECK_NEAR(p.z, 4.0f, 1e-3f);
}

static void test_hurt_and_invuln(void) {
    Player p; Hallway h; DummyPool dp; GameInput in = held(0);
    int ev;
    memset(&h, 0, sizeof(h)); memset(&dp, 0, sizeof(dp));
    player_init(&p);
    ev = player_update(&p, &in, &h, &dp, 1);
    CHECK(ev & PEV_HURT);
    ev = player_update(&p, &in, &h, &dp, 1);
    CHECK(!(ev & PEV_HURT));
    CHECK_EQ_INT(p.hearts, PLAYER_HEARTS - 1);
    p.invuln = 0;
    p.hearts = 1;
    player_update(&p, &in, &h, &dp, 2);
    CHECK(p.dead);
    CHECK_EQ_INT(p.hearts, 0);
}

static void test_recycle_remaps_dummy_doors(void) {
    Game g; GameInput in = held(0);
    int a, b, i;
    game_init(&g, 3);
    memset(&g.hall, 0, sizeof(g.hall));
    g.hall.ndoors = 6;
    for (i = 0; i < 6; i++) {
        g.hall.doors[i].z = i < 2 ? 0.5f + 0.3f * (float)i : 6.0f + (float)i;
        g.hall.doors[i].state = DOOR_CLOSED_FULL;
        g.hall.doors[i].remaining = 0;
    }
    g.hall.next_z = 50.0f;
    g.player.z = 5.0f;
    a = dummy_spawn(&g.dummies, DS_SMALL, 0.0f, 20.0f, 0);
    b = dummy_spawn(&g.dummies, DS_SMALL, 0.5f, 21.0f, 5);
    g.dummies.d[a].state = DST_DEAD;
    g.dummies.d[b].state = DST_DEAD;
    game_update(&g, &in);
    CHECK_EQ_INT(g.hall.recycled, 2);
    CHECK_EQ_INT(g.dummies.d[a].door, -1);
    CHECK_EQ_INT(g.dummies.d[b].door, 3);
}

static void test_door_pool_full_keeps_remaining(void) {
    Game g; GameInput in = held(0);
    int f, i, n = 0;
    game_init(&g, 5);
    setup_door(&g.hall, 3);
    for (i = 0; i < MAX_DUMMIES; i++) {
        dummy_spawn(&g.dummies, DS_SMALL, 0.0f, 30.0f + (float)i, -1);
        g.dummies.d[i].state = DST_DEAD;
    }
    for (f = 0; f < DOOR_SPAWN_INTERVAL + 20; f++) game_update(&g, &in);
    CHECK_EQ_INT(g.hall.doors[0].remaining, 3);
    CHECK_EQ_INT(g.hall.doors[0].state, DOOR_OPEN);
    g.dummies.d[0].alive = false;
    game_update(&g, &in);
    for (i = 0; i < MAX_DUMMIES; i++) if (g.dummies.d[i].alive && g.dummies.d[i].door == 0) n++;
    CHECK_EQ_INT(n, 1);
    CHECK_EQ_INT(g.hall.doors[0].remaining, 2);
}

static void test_back_floor_survives_recycle(void) {
    Game g; GameInput fwd = held(BTN_A), back = held(BTN_B);
    int f;
    float door_z;
    game_init(&g, 11);
    setup_door(&g.hall, 0);
    door_z = g.hall.doors[0].z;
    g.player.z = 4.4f; g.cam.player_z = 4.4f;
    for (f = 0; f < 400 && g.player.z < door_z + 6.0f; f++) game_update(&g, &fwd);
    CHECK(g.player.z >= door_z + 6.0f);
    CHECK_EQ_INT(g.hall.doors_emptied >= 1, 1);
    CHECK(g.hall.doors[0].z > door_z);   /* the closed door was recycled */
    for (f = 0; f < 600; f++) {
        game_update(&g, &back);
        CHECK(g.player.z >= door_z - 1e-4f);
    }
}

static void test_slam_with_emerging_and_no_remaining(void) {
    Game g; GameInput in = held(BTN_A);
    int f, i, n = 0, ev = 0;
    game_init(&g, 9);
    setup_door(&g.hall, 0);
    g.player.z = 4.5f; g.cam.player_z = 4.5f;
    i = dummy_spawn(&g.dummies, DS_SMALL, 1.3f, 4.55f, 0);
    for (f = 0; f < 3; f++) game_update(&g, &in);
    CHECK(g.player.z < 4.55f);                     /* slammed back */
    for (f = 0; f < MAX_DUMMIES; f++) n += g.dummies.d[f].alive;
    CHECK_EQ_INT(n, 1);                            /* nothing new spawned */
    CHECK_EQ_INT(g.hall.doors[0].state, DOOR_OPEN);
    while (g.dummies.d[i].state == DST_EMERGING) game_update(&g, &(GameInput){0});
    g.dummies.d[i].alive = false;
    for (f = 0; f < 120; f++) {
        ev |= player_update(&g.player, &in, &g.hall, &g.dummies, 0);
        if (ev & PEV_DOOR_CLOSED) break;
    }
    CHECK(ev & PEV_DOOR_CLOSED);
    CHECK_EQ_INT(g.hall.doors[0].state, DOOR_CLOSED_DONE);
}

void suite_player(void) {
    test_back_floor_survives_recycle();
    test_slam_with_emerging_and_no_remaining();
    test_player_walks();
    test_player_blocked_by_dummy();
    test_door_slam();
    test_door_slam_spawns_dummy();
    test_door_closes_when_empty();
    test_cant_walk_back_past_closed_door();
    test_hurt_and_invuln();
    test_recycle_remaps_dummy_doors();
    test_door_pool_full_keeps_remaining();
}
