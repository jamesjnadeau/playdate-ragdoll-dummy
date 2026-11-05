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


/* ---- Task 17: title, game over, sound ---- */

static int has_text(const RenderList* rl, const char* s) {
    int i;
    for (i = 0; i < rl->n; i++)
        if (rl->cmd[i].type == RC_TEXT && strcmp(rl->text + rl->cmd[i].text_off, s) == 0) return 1;
    return 0;
}

static int count_sound(const Game* g, SoundId id) {
    int i, n = 0;
    for (i = 0; i < g->nsounds; i++) n += g->sounds[i] == id;
    return n;
}

/* Runs frames until `frames` have passed, summing how often each sound was pushed. */
static void tally(int frames, const GameInput* in, int counts[SND_COUNT]) {
    int f, s;
    for (f = 0; f < frames; f++) {
        game_update(&g_, in);
        for (s = 0; s < SND_COUNT; s++) counts[s] += count_sound(&g_, (SoundId)s);
    }
}

static void test_title_to_play(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 7);
    g_.mode = MODE_TITLE;
    g_.bags = 9;
    g_.player.hearts = 1;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);   /* waits for a press */
    in.held = BTN_A;                      /* a hold is not a press */
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_EQ_INT(g_.player.hearts, PLAYER_HEARTS);
    CHECK_EQ_INT(g_.bags, 0);
    CHECK(!g_.player.dead);
}

static void test_title_b_goes_to_skins(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 7);
    g_.mode = MODE_TITLE;
    in.pressed = BTN_B;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_SKINS);
}

static void test_title_screen_contents(void) {
    int i, dummies = 0;
    game_init(&g_, 7);
    game_apply_bests(&g_, 12, 345);
    game_show_title(&g_);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "RAGDOLL DUMMIES"));
    CHECK(has_text(&g_.rl, "Press A to start"));
    CHECK(has_text(&g_.rl, "B: Skins"));
    CHECK(has_text(&g_.rl, "Best bags: 12"));
    CHECK(has_text(&g_.rl, "Best distance: 345m"));
    CHECK_EQ_INT(g_.rl.dropped, 0);
    CHECK(g_.rl.hud_open);
    for (i = 0; i < g_.rl.n; i++) {
        const RenderCmd* c = &g_.rl.cmd[i];
        if (c->type == RC_BITMAP && c->image >= IMG_DUMMY0 && c->image < IMG_WEAPON_HELD0) dummies++;
    }
    CHECK(dummies > 0);                       /* one idle dummy */
    CHECK(count_type(&g_.rl, RC_POLY) > 10);  /* the hallway behind it */
    CHECK_EQ_INT(g_.dummies.d[0].alive ? 1 : 0, 1);
    CHECK_NEAR(g_.dummies.d[0].z, TITLE_DUMMY_Z, 1e-4f);
}

static void test_restart_keeps_bests_and_rigs(void) {
    GameInput in;
    DummyRig rigs[DS_COUNT];
    int i;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 7);
    for (i = 0; i < DS_COUNT; i++) { rigs[i] = g_.rigs[i]; rigs[i].height_px += 3.0f; }
    game_set_rigs(&g_, rigs);
    game_apply_bests(&g_, 30, 400);
    game_show_title(&g_);
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_EQ_INT(g_.best_bags, 30);
    CHECK_NEAR(g_.best_distance, 400.0f, 1e-4f);
    CHECK_NEAR(g_.rigs[1].height_px, rigs[1].height_px, 1e-4f);
    CHECK_EQ_INT(g_.seen_new_best, 0);
    CHECK(!g_.save_dirty);
}

static void kill_player(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    g_.player.hearts = 1;
    g_.pending_damage = 5;
    game_update(&g_, &in);
}

static void test_death_to_game_over(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    game_apply_bests(&g_, 0, 0);
    g_.bags = 3;
    g_.player.hearts = 0;          /* forced */
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK(g_.save_dirty);          /* 3 bags beats 0 */
    CHECK_EQ_INT(g_.best_bags, 3);
    CHECK_EQ_INT(g_.seen_new_best, 1);
    /* damage taking the last heart does the same */
    start(3);
    game_apply_bests(&g_, 50, 5000);
    g_.bags = 2;
    kill_player();
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK(!g_.save_dirty);         /* no new best */
    CHECK_EQ_INT(g_.seen_new_best, 0);
}

static void test_game_over_screen(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    game_apply_bests(&g_, 1, 2);
    g_.bags = 7;
    g_.player.z = 25.0f;
    kill_player();
    game_render(&g_);
    CHECK(has_text(&g_.rl, "BAGGED: 7"));
    CHECK(has_text(&g_.rl, "Distance: 25m"));
    CHECK(has_text(&g_.rl, "Best bags: 7"));
    CHECK(has_text(&g_.rl, "Best distance: 25m"));
    CHECK(has_text(&g_.rl, "NEW BEST!"));
    CHECK_EQ_INT(g_.rl.dropped, 0);

    start(3);
    game_apply_bests(&g_, 99, 999);
    g_.bags = 7;
    kill_player();
    game_render(&g_);
    CHECK(has_text(&g_.rl, "BAGGED: 7"));
    CHECK(has_text(&g_.rl, "Best bags: 99"));
    CHECK(!has_text(&g_.rl, "NEW BEST!"));

    /* A leaves for the title, but only a press after the lock, so a walking player does not skip it */
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    memset(&in, 0, sizeof(in));
    { int i; for (i = 0; i < GAME_OVER_LOCK_FRAMES; i++) game_update(&g_, &in); }
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);
    CHECK_EQ_INT(g_.best_bags, 99);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "Press A to start"));
}

/* Bests never go down: a weaker run after a stronger one leaves them, and a death is saved only when a best improves. */
static void test_bests_survive_a_worse_run(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    game_apply_bests(&g_, 0, 0);
    g_.bags = 20;
    g_.player.z = 300.0f;
    kill_player();
    CHECK_EQ_INT(g_.best_bags, 20);
    CHECK_EQ_INT((int)g_.best_distance, 300);
    g_.save_dirty = false;
    /* back through the title into a new, worse run */
    { int i; for (i = 0; i < GAME_OVER_LOCK_FRAMES; i++) game_update(&g_, &in); }
    in.pressed = BTN_A;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);
    CHECK_EQ_INT(g_.best_bags, 20);
    g_.bags = 5;
    g_.player.z = 40.0f;
    kill_player();
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK_EQ_INT(g_.best_bags, 20);
    CHECK_EQ_INT((int)g_.best_distance, 300);
    CHECK(!g_.save_dirty);
    CHECK_EQ_INT(g_.seen_new_best, 0);
}

static void test_sound_events(void) {
    GameInput in;
    int counts[SND_COUNT];
    memset(&in, 0, sizeof(in));
    memset(counts, 0, sizeof(counts));
    start(3);
    in.crank_delta = 30.0f;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.nsounds, 1);
    CHECK_EQ_INT(count_sound(&g_, SND_SWING), 1);
    in.crank_delta = 0.0f;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.nsounds, 0);   /* cleared at the start of each update */
    tally(30, &in, counts);
    CHECK_EQ_INT(counts[SND_SWING], 0);   /* once per swing */
    /* cleared in the other modes too */
    g_.sounds[0] = SND_BURST; g_.nsounds = 1;
    g_.mode = MODE_OFFER;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.nsounds, 0);
}

/* A dummy in reach of a swing with `w`: returns the sounds the whole swing makes. */
static void swing_at_dummy(WeaponId w, float hp, int counts[SND_COUNT]) {
    GameInput in;
    int d, i;
    memset(&in, 0, sizeof(in));
    memset(counts, 0, SND_COUNT * sizeof(int));
    start(3);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    g_.weapon = w;
    d = dummy_spawn(&g_.dummies, DS_SMALL, 0.0f, g_.player.z + 0.9f, -1);
    g_.dummies.d[d].state = DST_STAGGER;
    g_.dummies.d[d].timer = 100;
    g_.dummies.d[d].hp = hp;
    in.crank_delta = 30.0f;
    for (i = 0; i < 24; i++) {
        int s;
        game_update(&g_, &in);
        for (s = 0; s < SND_COUNT; s++) counts[s] += count_sound(&g_, (SoundId)s);
        in.crank_delta = 0.0f;
        g_.dummies.d[d].timer = 100;
    }
}

static void test_sound_hit_light_and_heavy(void) {
    int c[SND_COUNT];
    swing_at_dummy(W_CHICKEN, 100.0f, c);
    CHECK_EQ_INT(c[SND_SWING], 1);
    CHECK_EQ_INT(c[SND_HIT_LIGHT], 1);
    CHECK_EQ_INT(c[SND_HIT_HEAVY], 0);
    CHECK_EQ_INT(c[SND_GRUNT], 0);
    swing_at_dummy(W_SLEDGE, 100.0f, c);
    CHECK_EQ_INT(c[SND_HIT_HEAVY], 1);
    CHECK_EQ_INT(c[SND_HIT_LIGHT], 0);
}

static void test_sound_grunt_on_knockout_and_bag_scored_at_once(void) {
    int c[SND_COUNT];
    swing_at_dummy(W_BAT, 0.1f, c);
    CHECK_EQ_INT(c[SND_GRUNT], 1);
    CHECK_EQ_INT(g_.bags >= 1 ? 1 : 0, 1);
}

/* The knockout bag is scored when the dummy goes down, so dying that same frame still counts it. */
static void test_death_frame_knockout_counts(void) {
    GameInput in;
    int d, i;
    memset(&in, 0, sizeof(in));
    start(3);
    game_apply_bests(&g_, 0, 0);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    g_.weapon = W_BAT;
    d = dummy_spawn(&g_.dummies, DS_SMALL, 0.0f, g_.player.z + 0.9f, -1);
    g_.dummies.d[d].state = DST_STAGGER;
    g_.dummies.d[d].timer = 100;
    g_.dummies.d[d].hp = 0.1f;
    in.crank_delta = 30.0f;
    game_update(&g_, &in);
    in.crank_delta = 0.0f;
    for (i = 0; i < 40 && !(g_.swing.phase == SWS_SWEEP && g_.swing.t == 2); i++) {
        g_.dummies.d[d].timer = 100;
        game_update(&g_, &in);
    }
    CHECK_EQ_INT(g_.swing.phase, SWS_SWEEP);
    CHECK_EQ_INT(g_.bags, 0);
    g_.player.hearts = 1;
    g_.pending_damage = 9;         /* the player dies on the very frame of the impact */
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK_EQ_INT(g_.bags, 1);
    CHECK_EQ_INT(g_.best_bags, 1); /* death handling ran after that frame's scoring */
    CHECK(g_.save_dirty);
}

/* A bowled-out dummy is scored when the ragdoll knocks it down, and grunts. */
static void test_bowled_knockout_scores_and_grunts(void) {
    GameInput in;
    int c[SND_COUNT], a, b, i;
    memset(&in, 0, sizeof(in));
    memset(c, 0, sizeof(c));
    start(3);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    a = dummy_spawn(&g_.dummies, DS_MEDIUM, 0.0f, g_.player.z + 1.5f, -1);
    b = dummy_spawn(&g_.dummies, DS_MEDIUM, 0.0f, g_.player.z + 2.4f, -1);
    g_.dummies.d[b].state = DST_STAGGER;
    g_.dummies.d[b].timer = 1000;
    g_.dummies.d[b].hp = 0.01f;
    game_knock_out(&g_, a, (Vec3){ 0.0f, 1.0f, 8.0f });
    CHECK_EQ_INT(g_.bags, 1);
    for (i = 0; i < 20; i++) { g_.dummies.d[b].timer = 1000; tally(1, &in, c); if (g_.dummies.d[b].state == DST_RAGDOLL) break; }
    CHECK_EQ_INT(g_.dummies.d[b].state, DST_RAGDOLL);
    CHECK(g_.dummies.d[b].scored);
    CHECK_EQ_INT(g_.bags, 2);
    CHECK_EQ_INT(c[SND_GRUNT], 1);
}

static void test_sound_burst(void) {
    GameInput in;
    int c[SND_COUNT], d, i;
    memset(&in, 0, sizeof(in));
    memset(c, 0, sizeof(c));
    start(3);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    d = dummy_spawn(&g_.dummies, DS_SMALL, 0.3f, g_.player.z + 2.0f, -1);
    game_knock_out(&g_, d, (Vec3){ 12.0f, 3.0f, 4.0f });
    CHECK_EQ_INT(g_.bags, 1);
    game_knock_out(&g_, d, (Vec3){ 12.0f, 3.0f, 4.0f });   /* once per dummy */
    CHECK_EQ_INT(g_.bags, 1);
    tally(200, &in, c);
    CHECK_EQ_INT(c[SND_BURST], 1);
    CHECK_EQ_INT(g_.bags, 2);
}

static void test_sound_steps(void) {
    GameInput in;
    int c[SND_COUNT];
    memset(&in, 0, sizeof(in));
    memset(c, 0, sizeof(c));
    start(3);
    tally(40, &in, c);
    CHECK_EQ_INT(c[SND_STEP], 0);          /* standing */
    in.held = BTN_A;
    tally(24, &in, c);
    CHECK_EQ_INT(c[SND_STEP], 2);          /* every 12 frames of walking */
    CHECK(g_.player.z > 0.5f);
}

static void test_sound_door_slam(void) {
    GameInput in;
    int c[SND_COUNT];
    Door* dr;
    memset(&in, 0, sizeof(in));
    memset(c, 0, sizeof(c));
    start(3);
    dr = &g_.hall.doors[0];
    dr->state = DOOR_OPEN; dr->remaining = 3; dr->spawn_timer = 999; dr->leaf_angle = 1.0f;
    g_.player.z = dr->z + DOOR_LEN * 0.5f - 0.02f;
    g_.cam.player_z = g_.player.z;
    in.held = BTN_A;
    tally(3, &in, c);
    CHECK_EQ_INT(c[SND_DOOR_SLAM], 1);
}

static void test_sound_queue_dedupes_and_is_bounded(void) {
    int i;
    start(3);
    for (i = 0; i < 40; i++) game_push_sound(&g_, SND_HIT_LIGHT);
    CHECK_EQ_INT(g_.nsounds, 1);   /* one player per sound: a repeat in the same frame adds nothing */
    for (i = 0; i < SND_COUNT; i++) game_push_sound(&g_, (SoundId)i);
    CHECK_EQ_INT(g_.nsounds, SND_COUNT);
    CHECK(SND_COUNT <= SOUND_QUEUE_MAX);
}

static void test_crank_hint(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    start(3);
    in.crank_docked = true;
    game_update(&g_, &in);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "Use the crank!"));
    CHECK_EQ_INT(g_.rl.dropped, 0);
    in.crank_docked = false;
    game_update(&g_, &in);
    game_render(&g_);
    CHECK(!has_text(&g_.rl, "Use the crank!"));
}

/* The game-over screen shows the bests after this run's update, whatever order the save was applied in. */
static void test_game_over_shows_updated_bests(void) {
    BenchConfig c;
    c.distance = 0.0f; c.seed = 1; c.scene = BENCH_GAMEOVER;
    game_start_bench(&g_, &c);
    game_apply_bests(&g_, 0, 0);   /* main applies the (older) save after the scene is built */
    game_render(&g_);
    CHECK(has_text(&g_.rl, "BAGGED: 14"));
    CHECK(has_text(&g_.rl, "Best bags: 14"));
    CHECK(has_text(&g_.rl, "Best distance: 120m"));
    CHECK(has_text(&g_.rl, "NEW BEST!"));
    /* the real death path */
    start(3);
    game_apply_bests(&g_, 2, 5);
    g_.bags = 14;
    g_.player.z = 120.0f;
    kill_player();
    game_render(&g_);
    CHECK(has_text(&g_.rl, "Best bags: 14"));
    CHECK(has_text(&g_.rl, "Best distance: 120m"));
}

static void test_death_on_pickup_frame_clears_pickup(void) {
    start(3);
    g_.pickup_z = g_.player.z + 0.1f;   /* triggers the offer this very frame */
    kill_player();
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK_NEAR(g_.pickup_z, -1.0f, 1e-6f);
}

static void test_bench_title_scenes(void) {
    BenchConfig c;
    c.distance = 0.0f; c.seed = 1;
    c.scene = BENCH_TITLE;
    game_start_bench(&g_, &c);
    CHECK_EQ_INT(g_.mode, MODE_TITLE);
    c.scene = BENCH_GAMEOVER;
    game_start_bench(&g_, &c);
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "NEW BEST!"));
    c.scene = BENCH_WALK;
    game_start_bench(&g_, &c);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);   /* the bench skips the title */
    kill_player();
    CHECK(g_.player.dead);
    CHECK_EQ_INT(g_.mode, MODE_PLAY);   /* and a benchmark scene is not cut short by dying */
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
    test_title_to_play();
    test_title_b_goes_to_skins();
    test_title_screen_contents();
    test_restart_keeps_bests_and_rigs();
    test_death_to_game_over();
    test_game_over_screen();
    test_bests_survive_a_worse_run();
    test_sound_events();
    test_sound_hit_light_and_heavy();
    test_sound_grunt_on_knockout_and_bag_scored_at_once();
    test_death_frame_knockout_counts();
    test_bowled_knockout_scores_and_grunts();
    test_sound_burst();
    test_sound_steps();
    test_sound_door_slam();
    test_sound_queue_dedupes_and_is_bounded();
    test_crank_hint();
    test_game_over_shows_updated_bests();
    test_death_on_pickup_frame_clears_pickup();
    test_bench_title_scenes();
}
