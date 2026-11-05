#include <string.h>
#include "test.h"
#include "combat.h"
#include "tuning.h"
#include "game.h"

static Swing mk_swing(SwingSide side, SwingVert vert, float factor) {
    Swing s;
    memset(&s, 0, sizeof s);
    s.side = side; s.vert = vert; s.factor = factor;
    return s;
}

static int put(DummyPool* p, DummySize sz, float x, float z) {
    int i = dummy_spawn(p, sz, x, z, -1);
    p->d[i].state = DST_WALK;
    return i;
}

static void test_hit_in_reach(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_CENTER, VERT_DOWN, 1.0f);
    memset(&p, 0, sizeof p);
    put(&p, DS_MEDIUM, 0.0f, 1.2f);
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4), 1);
    CHECK_NEAR(h[0].damage, 1.0f, 0.001f);
    CHECK(!h[0].head);
    CHECK_EQ_INT(p.d[0].state, DST_STAGGER);
    CHECK_EQ_INT(p.d[0].reaction_t, REACTION_FRAMES);
    CHECK_EQ_INT(p.d[0].reaction, RX_DOWNWARD);
    CHECK_NEAR(p.d[0].z, 1.2f + DUMMY_HIT_PUSHBACK, 0.001f);
}

static void test_head_zone_by_aim(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_CENTER, VERT_UP, 1.0f);
    memset(&p, 0, sizeof p);
    put(&p, DS_MEDIUM, 0.0f, 1.2f);
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 1, 0.0f, &p, h, 4), 1);
    CHECK(h[0].head);
    CHECK_NEAR(h[0].damage, 1.5f, 0.001f);
    memset(&p, 0, sizeof p);
    put(&p, DS_SMALL, 0.0f, 1.2f);
    combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4);
    CHECK(h[0].head);
    memset(&p, 0, sizeof p);
    put(&p, DS_LARGE, 0.0f, 1.2f);
    combat_resolve(&s, weapon_def(W_BAT), 2, 0.0f, &p, h, 4);
    CHECK(h[0].head);
}

static void test_out_of_reach(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_CENTER, VERT_DOWN, 1.0f);
    memset(&p, 0, sizeof p);
    put(&p, DS_MEDIUM, 0.0f, 1.7f);
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4), 0);
    p.d[0].z = -0.5f;   /* behind the player */
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4), 0);
}

static void test_side_arc(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_LEFT, VERT_DOWN, 1.0f);
    int l, r;
    memset(&p, 0, sizeof p);
    l = put(&p, DS_MEDIUM, -1.5f, 1.2f);
    r = put(&p, DS_MEDIUM, 1.5f, 1.2f);
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4), 1);
    CHECK_EQ_INT(h[0].dummy, l);
    CHECK_EQ_INT(p.d[r].reaction_t, 0);
}

static void test_hits_all_and_skips_ineligible(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_CENTER, VERT_DOWN, 1.0f);
    int a, b, e, rg;
    memset(&p, 0, sizeof p);
    a = put(&p, DS_SMALL, -0.3f, 1.0f);
    b = put(&p, DS_SMALL, 0.3f, 1.2f);
    e = dummy_spawn(&p, DS_SMALL, 0.0f, 1.0f, -1);   /* stays EMERGING */
    rg = put(&p, DS_SMALL, 0.0f, 1.1f);
    p.d[rg].state = DST_RAGDOLL;
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4), 2);
    CHECK_EQ_INT(h[0].dummy, a);
    CHECK_EQ_INT(h[1].dummy, b);
    CHECK_EQ_INT(p.d[e].reaction_t, 0);
    CHECK_EQ_INT(p.d[rg].reaction_t, 0);
    /* max_out caps the result */
    p.d[a].state = DST_WALK; p.d[b].state = DST_WALK;
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 1), 1);
}

static void test_knockout_launch(void) {
    DummyPool p; HitResult h[4]; Swing s = mk_swing(SIDE_LEFT, VERT_UP, 1.5f);
    int i;
    memset(&p, 0, sizeof p);
    i = put(&p, DS_SMALL, -0.6f, 1.2f);
    p.d[i].hp = 2.0f;
    CHECK_EQ_INT(combat_resolve(&s, weapon_def(W_SLEDGE), 1, 0.0f, &p, h, 4), 1);
    CHECK(h[0].knockout);
    CHECK(h[0].launch.z > 0.0f);
    CHECK(h[0].launch.x > 0.0f);
    CHECK(h[0].launch.y > 0.0f);
    CHECK_NEAR(v3_len(h[0].launch), 9.0f * 1.5f, 0.01f);
    CHECK_EQ_INT(p.d[i].state, DST_RAGDOLL);
    CHECK(p.d[i].alive);
    CHECK_NEAR(p.d[i].launch.z, h[0].launch.z, 0.0001f);
    /* centre swing flies straight */
    s = mk_swing(SIDE_CENTER, VERT_DOWN, 1.0f);
    memset(&p, 0, sizeof p);
    i = put(&p, DS_SMALL, 0.0f, 1.2f);
    p.d[i].hp = 0.5f;
    combat_resolve(&s, weapon_def(W_BAT), 0, 0.0f, &p, h, 4);
    CHECK_NEAR(h[0].launch.x, 0.0f, 0.0001f);
}

static void test_reaction_mapping(void) {
    CHECK_EQ_INT(combat_reaction(SIDE_CENTER, VERT_UP), RX_UPWARD);
    CHECK_EQ_INT(combat_reaction(SIDE_CENTER, VERT_DOWN), RX_DOWNWARD);
    CHECK_EQ_INT(combat_reaction(SIDE_LEFT, VERT_UP), RX_LEFT_UP);
    CHECK_EQ_INT(combat_reaction(SIDE_LEFT, VERT_DOWN), RX_LEFT_DOWN);
    CHECK_EQ_INT(combat_reaction(SIDE_RIGHT, VERT_UP), RX_RIGHT_UP);
    CHECK_EQ_INT(combat_reaction(SIDE_RIGHT, VERT_DOWN), RX_RIGHT_DOWN);
}

static void test_game_wiring_and_undock(void) {
    Game g; GameInput in; int f, i, hits = 0;
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    CHECK_EQ_INT(g.weapon, W_BAT);
    /* undock jump is swallowed */
    in.crank_docked = true;
    game_update(&g, &in);
    in.crank_docked = false; in.crank_delta = 90.0f;
    game_update(&g, &in);
    CHECK_EQ_INT(g.swing.phase, SWS_READY);
    /* a real swing at a dummy in reach reaches combat and reaction counts down */
    i = dummy_spawn(&g.dummies, DS_MEDIUM, 0.0f, 1.2f, -1);
    g.dummies.d[i].state = DST_WALK;
    for (f = 0; f < 30; f++) {
        in.crank_delta = f < 3 ? 30.0f : 0.0f;
        game_update(&g, &in);
        hits += g.nhits;
    }
    CHECK_EQ_INT(hits, 1);
    CHECK(g.dummies.d[i].hp < dummy_max_hp(DS_MEDIUM));
    CHECK_EQ_INT(g.dummies.d[i].reaction_t, 0);
    CHECK_EQ_INT(g.dummies.d[i].reaction, RX_NONE);
}

void suite_combat(void) {
    test_game_wiring_and_undock();
    test_hit_in_reach();
    test_head_zone_by_aim();
    test_out_of_reach();
    test_side_arc();
    test_hits_all_and_skips_ineligible();
    test_knockout_launch();
    test_reaction_mapping();
}
