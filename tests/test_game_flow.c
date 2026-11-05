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
    memset(&in, 0, sizeof(in));
    start(3);
    d = dummy_spawn(&g_.dummies, DS_MEDIUM, 0.5f, 5.0f, -1);
    g_.dummies.d[d].state = DST_WALK;
    enter_offer(W_PAN);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    for (i = 0; i < MAX_DUMMIES; i++) z[i] = g_.dummies.d[i].z;
    frame = g_.frame;
    in.held = BTN_A;
    in.crank_delta = 30.0f;
    for (i = 0; i < 30; i++) game_update(&g_, &in);
    for (i = 0; i < MAX_DUMMIES; i++) CHECK_NEAR(g_.dummies.d[i].z, z[i], 1e-6f);
    CHECK_EQ_INT(g_.frame, frame);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
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
        if (c->type == RC_RECT && c->pattern == PAT_D50 && c->sx >= SCREEN_W && c->sy >= SCREEN_H) rects50++;
    }
    CHECK_EQ_INT(rects50, 1);
    CHECK(count_type(&g_.rl, RC_TEXT) >= 4);
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
    test_bench_offer_scene();
    test_hud_offer_panel();
}
