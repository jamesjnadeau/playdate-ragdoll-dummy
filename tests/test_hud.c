#include <string.h>
#include "test.h"
#include "game.h"
#include "hud.h"
#include "tuning.h"

static Game g_;

static int has_text(const RenderList* rl, const char* s) {
    int i;
    for (i = 0; i < rl->n; i++)
        if (rl->cmd[i].type == RC_TEXT && strcmp(rl->text + rl->cmd[i].text_off, s) == 0) return 1;
    return 0;
}

static int count_image(const RenderList* rl, int img) {
    int i, n = 0;
    for (i = 0; i < rl->n; i++) n += rl->cmd[i].type == RC_BITMAP && rl->cmd[i].image == img;
    return n;
}

static void test_hud_text(void) {
    RenderList rl;
    game_init(&g_, 1);
    g_.bags = 12;
    g_.player.z = 48.7f;
    rl_clear(&rl);
    hud_draw_play(&rl, &g_);
    CHECK(has_text(&rl, "x12"));
    CHECK(has_text(&rl, "48m"));
    CHECK(has_text(&rl, "Bat"));
    CHECK(has_text(&rl, "SPD"));
    CHECK(has_text(&rl, "PWR"));
    CHECK_EQ_INT(rl.dropped, 0);
}

static void test_hud_hearts(void) {
    RenderList rl;
    game_init(&g_, 1);
    g_.player.hearts = 3;
    rl_clear(&rl);
    hud_draw_play(&rl, &g_);
    CHECK_EQ_INT(count_image(&rl, IMG_UI_HEART_FULL), 3);
    CHECK_EQ_INT(count_image(&rl, IMG_UI_HEART_EMPTY), 2);
    CHECK_EQ_INT(count_image(&rl, IMG_UI_BAG), 1);
}

static void test_hud_fits_reserve_with_offer(void) {
    RenderList rl;
    int hud_only;
    game_init(&g_, 1);
    g_.held = BTN_LEFT;
    g_.bags = 123456;
    g_.player.z = 99999.0f;
    rl_clear(&rl);
    hud_draw_play(&rl, &g_);
    hud_only = rl.n;
    hud_draw_offer(&rl, W_BAT, W_SLEDGE);
    CHECK(rl.n <= RL_HUD_RESERVE);
    CHECK(hud_only > 10);
    CHECK_EQ_INT(rl.dropped, 0);
}

static void test_hud_indicator_side(void) {
    RenderList a, b, c;
    int i, left = 0, right = 0;
    game_init(&g_, 1);
    g_.held = 0;
    rl_clear(&a); hud_draw_play(&a, &g_);
    g_.held = BTN_LEFT;
    rl_clear(&b); hud_draw_play(&b, &g_);
    g_.held = BTN_RIGHT;
    rl_clear(&c); hud_draw_play(&c, &g_);
    CHECK_EQ_INT(b.n, a.n + 1);   /* the triangle */
    CHECK_EQ_INT(c.n, a.n + 1);
    for (i = 0; i < b.n; i++) if (b.cmd[i].type == RC_POLY) left = (int)b.cmd[i].pts[0];
    for (i = 0; i < c.n; i++) if (c.cmd[i].type == RC_POLY) right = (int)c.cmd[i].pts[0];
    CHECK(left > 0 && right > left);
}

static void test_hud_in_game_render(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 2);
    game_update(&g_, &in);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "0m"));
    g_.offer = W_SLEDGE;
    g_.pickup_z = g_.player.z + 0.3f;
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.mode, MODE_OFFER);
    game_render(&g_);
    CHECK(has_text(&g_.rl, "0m"));
    CHECK(has_text(&g_.rl, "A new weapon!"));
}

static void test_score_burst_bonus(void) {
    GameInput in;
    int d, i, seen_ko = 0;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 5);
    game_update(&g_, &in);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    CHECK_EQ_INT(g_.bags, 0);
    d = dummy_spawn(&g_.dummies, DS_SMALL, 0.3f, g_.player.z + 2.0f, -1);
    game_knock_out(&g_, d, (Vec3){ 12.0f, 3.0f, 4.0f });
    game_update(&g_, &in);
    CHECK_EQ_INT(g_.bags, 1);   /* the knockout */
    seen_ko = g_.bags;
    for (i = 0; i < 150 && g_.bags < 2; i++) game_update(&g_, &in);
    CHECK_EQ_INT(seen_ko, 1);
    CHECK_EQ_INT(g_.bags, 2);   /* the burst */
    for (i = 0; i < 60; i++) game_update(&g_, &in);
    CHECK_EQ_INT(g_.bags, 2);   /* each only once */
}

static void test_score_knockout_by_swing_counts_once(void) {
    GameInput in;
    int d, i;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 5);
    game_update(&g_, &in);
    for (i = 0; i < g_.hall.ndoors; i++) g_.hall.doors[i].remaining = 0;
    d = dummy_spawn(&g_.dummies, DS_SMALL, 0.0f, g_.player.z + 2.0f, -1);
    game_knock_out(&g_, d, (Vec3){ 0.0f, 0.5f, 0.2f });
    for (i = 0; i < 3; i++) game_update(&g_, &in);
    CHECK_EQ_INT(g_.bags, 1);
}

static void test_death_updates_bests(void) {
    GameInput in;
    memset(&in, 0, sizeof(in));
    game_init(&g_, 3);
    game_apply_bests(&g_, 10, 100);
    CHECK(!g_.save_dirty);
    g_.bags = 4;
    g_.player.z = 50.0f;
    g_.player.hearts = 1;
    g_.pending_damage = 5;
    game_update(&g_, &in);
    CHECK(g_.player.dead);
    CHECK_EQ_INT(g_.mode, MODE_GAME_OVER);
    CHECK(!g_.save_dirty);          /* no new best: nothing to write */
    CHECK_EQ_INT(g_.best_bags, 10);
    CHECK_EQ_INT(g_.seen_new_best, 0);

    game_init(&g_, 3);
    game_apply_bests(&g_, 10, 100);
    g_.bags = 11;
    g_.player.z = 150.0f;
    g_.player.hearts = 1;
    g_.pending_damage = 5;
    game_update(&g_, &in);
    CHECK(g_.save_dirty);
    CHECK_EQ_INT(g_.best_bags, 11);
    CHECK_EQ_INT((int)g_.best_distance, 150);
    CHECK_EQ_INT(g_.seen_new_best, 1);
    g_.save_dirty = false;
    game_update(&g_, &in);          /* death is handled once */
    CHECK(!g_.save_dirty);
}

void suite_hud(void) {
    test_hud_text();
    test_hud_hearts();
    test_hud_fits_reserve_with_offer();
    test_hud_indicator_side();
    test_hud_in_game_render();
    test_score_burst_bonus();
    test_score_knockout_by_swing_counts_once();
    test_death_updates_bests();
}
