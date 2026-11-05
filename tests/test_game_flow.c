#include <string.h>
#include "test.h"
#include "game.h"
#include "hud.h"
#include "draw_world.h"
#include "tuning.h"

static Game g_;

static void start(uint32_t seed) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    game_init(&g_, seed);
    game_update(&g_, &in);   /* generates doors */
}

static void enter_offer(WeaponId offer) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    g_.offer = offer;
    g_.pickup_z = g_.player.z + 0.3f;
    game_update(&g_, &in);
}

static int count_type(const RenderList* rl, int type) {
    int i, n = 0;
    for (i = 0; i < rl->n; i++) n += rl->cmd[i].type == type;
    return n;
}

static void test_offer_after_4_doors(void) {
    GameInput in;
    uint32_t seed;
    memset(&in, 0, sizeof(in));
    start(3);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_NEAR(g_.pickup_z, -1.0f, 1e-6f);
    g_.hall.doors_emptied = 3;
    game_update(&g_, &in);
    CHECK_NEAR(g_.pickup_z, -1.0f, 1e-6f);
    g_.hall.doors_emptied = 4;
    game_update(&g_, &in);
    CHECK_NEAR(g_.pickup_z, g_.player.z + 3.0f, 1e-4f);
    CHECK(g_.offer != g_.weapon);
    CHECK(g_.offer >= 0 && g_.offer < W_COUNT);
    /* it is not re-placed every frame */
    g_.pickup_z += 0.5f;
    game_update(&g_, &in);
    CHECK_NEAR(g_.pickup_z, 3.5f, 1e-4f);
    /* every seed offers a different weapon */
    for (seed = 1; seed < 40; seed++) {
        start(seed);
        g_.weapon = (WeaponId)(seed % W_COUNT);
        g_.hall.doors_emptied = 4;
        game_update(&g_, &in);
        CHECK(g_.pickup_z > 0.0f);
        CHECK(g_.offer != g_.weapon);
    }
}

static void test_offer_take(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    CHECK_EQ_INT(g_.weapon, W_BAT);
    enter_offer(W_SLEDGE);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.weapon, W_SLEDGE);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_NEAR(g_.pickup_z, -1.0f, 1e-6f);
}

static void test_offer_keep(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    enter_offer(W_PAN);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    in.pressed = BTN_B;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.weapon, W_BAT);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_NEAR(g_.pickup_z, -1.0f, 1e-6f);
}

static void test_offer_acts_on_press_not_hold(void) {
    GameInput in;
    int i;
    memset(&in, 0, sizeof(in));
    start(3);
    enter_offer(W_PAN);
    in.held = BTN_A | BTN_B;   /* held but not newly pressed */
    for (i = 0; i < 5; i++) game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    CHECK_EQ_INT(g_.weapon, W_BAT);
}

static void test_pickup_far_does_not_trigger(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    g_.offer = W_PAN;
    g_.pickup_z = g_.player.z + 3.0f;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK(g_.pickup_z > 0.0f);
}

static void test_offer_pauses_world(void) {
    GameInput in;
    float z[MAX_DUMMIES];
    int i, d, frame;
    RagdollPool rp;
    ParticlePool pp;
    Impact imp[MAX_IMPACTS];
    Swing sw;
    int hurt;
    memset(&in, 0, sizeof(in));
    start(3);
    d = dummy_spawn(&g_.dummies, DS_MEDIUM, 0.5f, 5.0f, -1);
    g_.dummies.d[d].state = DST_WALK;
    enter_offer(W_PAN);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    for (i = 0; i < MAX_DUMMIES; i++) z[i] = g_.dummies.d[i].z;
    frame = g_.frame;
    g_.ragdolls.r[0].active = true;   /* something to move if the world were not paused */
    g_.ragdolls.r[0].p[0].y = 2.0f; g_.ragdolls.r[0].prev[0].y = 2.0f;
    g_.particles.p[0].active = true; g_.particles.p[0].life = 50; g_.particles.p[0].p.y = 2.0f;
    g_.impacts[0].t = 3; g_.hurt_t = 4;
    memcpy(&rp, &g_.ragdolls, sizeof(rp)); memcpy(&pp, &g_.particles, sizeof(pp));
    memcpy(imp, g_.impacts, sizeof(imp)); memcpy(&sw, &g_.swing, sizeof(sw)); hurt = g_.hurt_t;
    in.held = BTN_A;
    in.crank_delta = 30.0f;
    for (i = 0; i < 30; i++) game_update(&g_, &in);
    for (i = 0; i < MAX_DUMMIES; i++) CHECK_NEAR(g_.dummies.d[i].z, z[i], 1e-6f);
    CHECK_EQ_INT(g_.frame, frame);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    CHECK(memcmp(&rp, &g_.ragdolls, sizeof(rp)) == 0);
    CHECK(memcmp(&pp, &g_.particles, sizeof(pp)) == 0);
    CHECK(memcmp(imp, g_.impacts, sizeof(imp)) == 0);
    CHECK(memcmp(&sw, &g_.swing, sizeof(sw)) == 0);
    CHECK_EQ_INT(g_.hurt_t, hurt);
}

static void test_pickup_is_not_solid(void) {
    GameInput in;
    int i;
    memset(&in, 0, sizeof(in));
    start(3);
    g_.pickup_z = g_.player.z + 3.0f;
    g_.offer = W_PAN;
    in.held = BTN_A;
    for (i = 0; i < 20; i++) game_update(&g_, &in);
    CHECK(g_.player.z > 0.5f);   /* walked toward it unhindered */
}

static void test_pickup_drawn(void) {
    RenderList rl;
    Camera c;
    int i, found = 0;
    memset(&c, 0, sizeof(c));
    rl_clear(&rl);
    draw_pickup(&rl, &c, 3.0f, IMG_WEAPON_ICON0 + W_PAN);
    for (i = 0; i < rl.n; i++)
        if (rl.cmd[i].type == RC_BITMAP && rl.cmd[i].image == IMG_WEAPON_ICON0 + W_PAN) found++;
    CHECK_EQ_INT(found, 1);
    CHECK(rl.cmd[0].y > HORIZON_Y);   /* on the floor, below the horizon */
    rl_clear(&rl);
    draw_pickup(&rl, &c, -5.0f, IMG_WEAPON_ICON0);   /* behind the camera: nothing */
    CHECK_EQ_INT(rl.n, 0);
}

static void test_game_render_pickup_and_offer(void) {
    int i, rects50 = 0, icons = 0;
    start(3);
    g_.offer = W_CHICKEN;
    g_.pickup_z = g_.player.z + 3.0f;
    game_render(&g_);
    for (i = 0; i < g_.rl.n; i++)
        if (g_.rl.cmd[i].type == RC_BITMAP && g_.rl.cmd[i].image == IMG_WEAPON_ICON0 + W_CHICKEN) icons++;
    CHECK_EQ_INT(icons, 1);
    enter_offer(W_CHICKEN);
    game_render(&g_);
    for (i = 0; i < g_.rl.n; i++) {
        const RenderCmd* c = &g_.rl.cmd[i];
        if (c->type == RC_RECT && c->pattern == PAT_DIM && c->sx >= SCREEN_W && c->sy >= SCREEN_H) rects50++;
    }
    CHECK_EQ_INT(rects50, 1);
    CHECK(count_type(&g_.rl, RC_TEXT) >= 4);
}

/* Worst case: 16 dummy slots (10 live, 6 ragdolls), full particle pool, 8 impacts, offer panel. */
static void test_offer_panel_survives_busy_frame(void) {
    GameInput in;
    int i, texts = 0, bars = 0, icons = 0, dim = 0;
    memset(&in, 0, sizeof(in));
    start(3);
    for (i = 0; i < MAX_DUMMIES; i++) {
        int d = dummy_spawn(&g_.dummies, (DummySize)(i % DS_COUNT), -1.0f + 0.2f * (float)(i % 10), 3.0f + 0.5f * (float)i, -1);
        g_.dummies.d[d].state = i < MAX_LIVE_DUMMIES ? DST_WALK : DST_RAGDOLL;
        g_.dummies.d[d].launch.x = 0.5f;
    }
    game_update(&g_, &in);   /* spawns the ragdolls */
    for (i = 0; i < PARTICLE_MAX; i++) {
        Particle* p = &g_.particles.p[i];
        p->active = true; p->kind = (ParticleKind)(i % 3); p->variant = (uint8_t)(i % 4);
        p->p.x = -1.2f + 0.01f * (float)i; p->p.y = 0.5f + 0.005f * (float)i; p->p.z = g_.player.z + 2.0f + 0.03f * (float)i;
        p->life = 100;
    }
    for (i = 0; i < MAX_IMPACTS; i++) { g_.impacts[i].t = 3; g_.impacts[i].x = 50.0f + 40.0f * (float)i; g_.impacts[i].y = 100.0f; g_.impacts[i].r = 20.0f; }
    g_.weapon = W_BAT;
    g_.offer = W_PAN;
    g_.pickup_z = g_.player.z;
    g_.mode = MODE_OFFER;
    game_render(&g_);
    CHECK_EQ_INT(g_.rl.dropped, 0);
    for (i = 0; i < g_.rl.n; i++) {
        const RenderCmd* c = &g_.rl.cmd[i];
        if (c->type == RC_RECT && c->pattern == PAT_DIM) dim++;
        if (!dim) continue;   /* only what the panel drew, after the dim */
        texts += c->type == RC_TEXT;
        icons += c->type == RC_BITMAP && (c->image == IMG_WEAPON_ICON0 + W_BAT || c->image == IMG_WEAPON_ICON0 + W_PAN);
        bars += c->type == RC_RECT && c->sx < 20.0f;
    }
    CHECK_EQ_INT(texts, 9);
    CHECK_EQ_INT(icons, 2);
    CHECK_EQ_INT(bars, 12 * 2);
    CHECK_EQ_INT(dim, 1);
    CHECK(g_.rl.n > 300);   /* the scene really was busy */
}

/* A world heavy enough to hit the particle cap must still leave room for the 8 impact stars and the weapon. */
static void test_particle_cap_leaves_room_for_weapon_and_impacts(void) {
    int i, weapon = 0, polys = 0;
    start(3);
    for (i = 0; i < PARTICLE_MAX; i++) {
        Particle* p = &g_.particles.p[i];
        p->active = true; p->kind = PK_PUFF; p->variant = (uint8_t)(i % 4);
        p->p.x = -1.2f + 0.01f * (float)i; p->p.y = 0.5f; p->p.z = g_.player.z + 2.0f + 0.03f * (float)i;
        p->life = 100;
    }
    rl_clear(&g_.rl);
    for (i = 0; i < 600; i++) rl_rect(&g_.rl, 0.0f, 0.0f, 1.0f, 1.0f, PAT_BLACK, false);   /* a very heavy world */
    draw_particles(&g_.rl, &g_.cam, &g_.particles);
    CHECK(g_.rl.n > 600);   /* some particles did draw ... */
    CHECK(g_.rl.n <= RL_MAX_CMDS - RL_HUD_RESERVE - PARTICLE_CMD_RESERVE);   /* ... but only up to the world ceiling minus the reserve */
    for (i = 0; i < MAX_IMPACTS; i++) draw_impact(&g_.rl, 50.0f + 40.0f * (float)i, 100.0f, 20.0f);
    draw_weapon(&g_.rl, &g_.swing, &g_.wrigs[g_.weapon], IMG_WEAPON_HELD0 + (int)g_.weapon);
    CHECK_EQ_INT(g_.rl.dropped, 0);
    for (i = 0; i < g_.rl.n; i++) {
        weapon += g_.rl.cmd[i].type == RC_BITMAP && g_.rl.cmd[i].image == IMG_WEAPON_HELD0 + (int)g_.weapon;
        polys += g_.rl.cmd[i].type == RC_POLY;
    }
    CHECK_EQ_INT(weapon, 1);
    CHECK(polys >= MAX_IMPACTS * 2);
}

static void test_hud_mode_open_after_render_in_play(void) {
    start(3);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    game_render(&g_);
    CHECK(g_.rl.hud_open);   /* anything Task 16 draws after game_render's world is safe */
}

static void test_bench_offer_scene(void) {
    BenchConfig c;
    GameInput in;
    memset(&in, 0, sizeof(in));
    c.scene = BENCH_OFFER; c.distance = 0.0f; c.seed = 1;
    game_start_bench(&g_, &c);
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    CHECK_EQ_INT(g_.offer, W_SLEDGE);
}

static void test_hud_offer_panel(void) {
    RenderList rl;
    int i, icon_cur = 0, icon_off = 0, has_cur = 0, has_off = 0, filled = 0;
    rl_clear(&rl);
    hud_draw_offer(&rl, W_BAT, W_SLEDGE);
    for (i = 0; i < rl.n; i++) {
        const RenderCmd* c = &rl.cmd[i];
        if (c->type == RC_BITMAP && c->image == IMG_WEAPON_ICON0 + W_BAT) icon_cur++;
        if (c->type == RC_BITMAP && c->image == IMG_WEAPON_ICON0 + W_SLEDGE) icon_off++;
        if (c->type == RC_TEXT && strcmp(rl.text + c->text_off, "Bat") == 0) has_cur = 1;
        if (c->type == RC_TEXT && strcmp(rl.text + c->text_off, "Sledgehammer") == 0) has_off = 1;
        if (c->type == RC_RECT && c->pattern == PAT_BLACK && c->sx < 20.0f) filled++;
    }
    CHECK_EQ_INT(icon_cur, 1);
    CHECK_EQ_INT(icon_off, 1);
    CHECK(has_cur && has_off);
    /* filled bars: bat 4+3, sledgehammer 1+6 */
    CHECK_EQ_INT(filled, 4 + 3 + 1 + 6);
    CHECK_EQ_INT(rl.dropped, 0);
}

void suite_game_flow(void) {
    test_offer_after_4_doors();
    test_offer_take();
    test_offer_keep();
    test_offer_acts_on_press_not_hold();
    test_pickup_far_does_not_trigger();
    test_offer_pauses_world();
    test_pickup_is_not_solid();
    test_pickup_drawn();
    test_game_render_pickup_and_offer();
    test_offer_panel_survives_busy_frame();
    test_particle_cap_leaves_room_for_weapon_and_impacts();
    test_hud_mode_open_after_render_in_play();
    test_bench_offer_scene();
    test_hud_offer_panel();
}
