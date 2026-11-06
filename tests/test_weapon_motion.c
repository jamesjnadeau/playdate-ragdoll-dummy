#include <stdio.h>
#include <string.h>
#include "test.h"
#include "tuning.h"
#include "weapon_motion.h"

static GameInput inp(float delta, float angle, bool docked, uint8_t held) {
    GameInput in = {0};
    in.crank_delta = delta;
    in.crank_angle = angle;
    in.crank_docked = docked;
    in.held = held;
    return in;
}

static void crank(Weapon* wp, const WeaponDef* w, float delta, int frames) {
    for (int i = 0; i < frames; i++) {
        GameInput in = inp(delta, 0.0f, false, 0);
        weapon_update(wp, &in, w);
    }
}

static void start(Weapon* wp, const WeaponDef* w, float angle) {
    *wp = (Weapon){0};
    GameInput in = inp(0.0f, angle, false, 0);
    weapon_update(wp, &in, w);
}

/* Catch-up frames after turning 20/frame for 30 frames; -1 if never settles by 80. */
static int catch_up(WeaponId id, float* max_over, float* lag30) {
    const WeaponDef* w = weapon_def(id);
    Weapon wp;
    start(&wp, w, 0.0f);
    int first = -1;
    float over = 0.0f;
    for (int f = 1; f <= 80; f++) {
        GameInput in = inp(f <= 30 ? 20.0f : 0.0f, 0.0f, false, 0);
        weapon_update(&wp, &in, w);
        if (f == 30 && lag30) *lag30 = wp.target - wp.angle;
        if (f > 30 && wp.angle - wp.target > over) over = wp.angle - wp.target;
        bool ok = fabsf(wp.target - wp.angle) < 2.0f && fabsf(wp.vel) < 1.0f;
        if (ok) { if (first < 0) first = f - 30; }
        else first = -1;
    }
    if (max_over) *max_over = over;
    return first;
}

static void test_zero_is_docked_upright(void) {
    Weapon wp = {0};
    GameInput in = inp(0.0f, 0.0f, true, 0);
    weapon_update(&wp, &in, weapon_def(W_BAT));
    CHECK_NEAR(wp.angle, 0.0f, 0.0f);
    CHECK(!wp.attached);
    CHECK(!wp.swept);
}

static void test_undock_teleports(void) {
    Weapon wp = {0};
    GameInput in = inp(90.0f, 90.0f, false, 0);
    weapon_update(&wp, &in, weapon_def(W_BAT));
    CHECK_NEAR(wp.angle, 90.0f, 0.0f);
    CHECK_NEAR(wp.prev_angle, 90.0f, 0.0f);
    CHECK(wp.attached);
    CHECK(!wp.swept);
    CHECK_NEAR(wp.vel, 0.0f, 0.0f);
}

static void test_moves_same_frame(void) {
    for (int i = 0; i < W_COUNT; i++) {
        const WeaponDef* w = weapon_def((WeaponId)i);
        Weapon wp;
        start(&wp, w, 0.0f);
        crank(&wp, w, 5.0f, 1);
        CHECK(wp.vel != 0.0f);
        CHECK(wp.swept);
    }
}

static void test_catch_up_targets(void) {
    CHECK(catch_up(W_CHICKEN, 0, 0) >= 0 && catch_up(W_CHICKEN, 0, 0) <= 2);
    CHECK(catch_up(W_BAT, 0, 0) >= 0 && catch_up(W_BAT, 0, 0) <= 4);
    CHECK(catch_up(W_PAN, 0, 0) >= 0 && catch_up(W_PAN, 0, 0) <= 10);
    CHECK(catch_up(W_SLEDGE, 0, 0) >= 0 && catch_up(W_SLEDGE, 0, 0) <= 24);
}

static void test_heavier_lags_more(void) {
    float lag[W_COUNT];
    for (int i = 0; i < W_COUNT; i++) catch_up((WeaponId)i, 0, &lag[i]);
    CHECK(lag[W_CHICKEN] < 0.5f);
    CHECK(lag[W_CHICKEN] < lag[W_BAT]);
    CHECK(lag[W_BAT] < lag[W_PAN]);
    CHECK(lag[W_PAN] < lag[W_SLEDGE]);
    CHECK(lag[W_SLEDGE] > 55.0f);      /* the sledge trails the crank by a wide margin */
}

static void test_sledge_overshoots_slightly(void) {
    float over = 0.0f;
    catch_up(W_SLEDGE, &over, 0);
    CHECK(over > 0.0f && over < 16.0f);
}

static void test_vel_clamped(void) {
    const WeaponDef* w = weapon_def(W_SLEDGE);
    Weapon wp;
    start(&wp, w, 0.0f);
    crank(&wp, w, 720.0f, 1);
    CHECK(fabsf(wp.vel) <= WEAPON_MAX_VEL);
}

static void test_loose_slows_and_stops(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp;
    start(&wp, w, 90.0f);
    weapon_detach(&wp, 30.0f, -16.0f);
    float v = -16.0f;
    int stopped = -1;
    for (int i = 0; i < 30; i++) {
        crank(&wp, w, 0.0f, 1);
        v *= WEAPON_BOUNCE_DAMP;
        if (fabsf(v) < WEAPON_STOP_VEL) v = 0.0f;
        CHECK_NEAR(wp.vel, v, 0.001f);
        CHECK(!wp.attached);
        if (wp.vel == 0.0f && stopped < 0) stopped = i;
    }
    CHECK(stopped >= 0);
    CHECK_NEAR(wp.vel, 0.0f, 0.0f);
}

static void loose_at_30(Weapon* wp, const WeaponDef* w) {
    start(wp, w, 40.0f);
    weapon_detach(wp, 30.0f, -10.0f);
}

static void test_reconnect_turning_back(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp;
    loose_at_30(&wp, w);
    int n = 0;
    while (!wp.attached && n < 20) {
        crank(&wp, w, -5.0f, 1);
        n++;
    }
    CHECK(wp.attached);
    CHECK_NEAR(wp.angle, wp.target, 0.001f);
    CHECK(wp.target <= 30.0f + 0.001f + 5.0f);
    CHECK(n < 20);
}

static void test_reconnect_full_turn_forward(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp;
    loose_at_30(&wp, w);
    int n = 0;
    while (!wp.attached && n < 100) {
        crank(&wp, w, 10.0f, 1);
        n++;
    }
    CHECK(wp.attached);
    CHECK(n >= 25 && n <= 40);
    CHECK(fabsf(weapon_wrap_deg(wp.target - wp.angle)) < 1.0f);
    CHECK(fabsf(wp.target - wp.angle) < 1.0f);
}

static void test_no_false_reconnect_at_seam(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp;
    start(&wp, w, 0.0f);
    wp.target = 170.0f;
    weapon_detach(&wp, 0.0f, 0.0f);
    CHECK_NEAR(wp.prev_diff, 170.0f, 0.001f);
    crank(&wp, w, 5.0f, 3);
    CHECK(!wp.attached);
}

static void test_pivot_shift_eases(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp = {0};
    GameInput in = inp(0.0f, 0.0f, true, BTN_LEFT);
    for (int i = 0; i < 5; i++) weapon_update(&wp, &in, w);
    CHECK_NEAR(wp.shift, -50.0f, 0.001f);
    CHECK_NEAR(weapon_pivot_x(&wp), 150.0f, 0.001f);
    for (int i = 0; i < 15; i++) weapon_update(&wp, &in, w);
    CHECK_NEAR(wp.shift, -WEAPON_SHIFT_PX, 0.001f);
    in.held = BTN_LEFT | BTN_RIGHT;
    weapon_update(&wp, &in, w);
    CHECK_NEAR(wp.shift, -90.0f, 0.001f);
    in.held = 0;
    for (int i = 0; i < 20; i++) weapon_update(&wp, &in, w);
    CHECK_NEAR(wp.shift, 0.0f, 0.001f);
    in.held = BTN_RIGHT;
    for (int i = 0; i < 20; i++) weapon_update(&wp, &in, w);
    CHECK_NEAR(weapon_pivot_x(&wp), 300.0f, 0.001f);
}

static void test_dock_mid_bounce_resets(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    Weapon wp;
    start(&wp, w, 0.0f);
    weapon_detach(&wp, 30.0f, -16.0f);
    crank(&wp, w, 0.0f, 2);
    GameInput d = inp(0.0f, 0.0f, true, 0);
    weapon_update(&wp, &d, w);
    CHECK_NEAR(wp.angle, 0.0f, 0.0f);
    CHECK_NEAR(wp.vel, 0.0f, 0.0f);
    CHECK(!wp.live);
    GameInput u = inp(5.0f, 45.0f, false, 0);
    weapon_update(&wp, &u, w);
    CHECK_NEAR(wp.angle, 45.0f, 0.0f);
    CHECK(!wp.swept);
}

static void test_long_session_stays_precise(void) {
    const WeaponDef* w = weapon_def(W_CHICKEN);
    Weapon wp;
    start(&wp, w, 0.0f);
    crank(&wp, w, 40.0f, 20000);
    CHECK(fabsf(weapon_wrap_deg(wp.target - wp.angle)) < 0.5f);
    CHECK(fabsf(wp.angle) <= WEAPON_REBASE_DEG + 360.0f);
}

/* The weapons table (Task 1). */
static void test_weapons_table(void) {
    CHECK(weapon_def(W_SLEDGE) != NULL);
    CHECK_EQ_INT(weapon_def(W_SLEDGE)->power_bars, 6);
    CHECK_NEAR(weapon_def(W_CHICKEN)->mass, 0.4f, 1e-6f);
    CHECK_NEAR(weapon_def(W_BAT)->mass, 1.0f, 1e-6f);
    CHECK_NEAR(weapon_def(W_PAN)->mass, 1.4f, 1e-6f);
    CHECK_NEAR(weapon_def(W_SLEDGE)->mass, 2.6f, 1e-6f);
    CHECK(weapon_def(W_CHICKEN)->mass < weapon_def(W_BAT)->mass);
    CHECK(weapon_def(W_BAT)->mass < weapon_def(W_PAN)->mass);
    CHECK(weapon_def(W_PAN)->mass < weapon_def(W_SLEDGE)->mass);
    CHECK_EQ_INT(weapon_def(W_CHICKEN)->weight_bars, 1);
    CHECK_EQ_INT(weapon_def(W_BAT)->weight_bars, 2);
    CHECK_EQ_INT(weapon_def(W_PAN)->weight_bars, 3);
    CHECK_EQ_INT(weapon_def(W_SLEDGE)->weight_bars, 6);
    for (int f = 0; f < W_COUNT; f++) {
        const WeaponDef* d = weapon_def((WeaponId)f);
        CHECK(d->damp > 0.0f && d->damp <= 1.0f);
        CHECK(d->spring > 0.0f);
    }
    CHECK(strcmp(weapon_def(W_CHICKEN)->name, "Rubber Chicken") == 0);
    CHECK(strcmp(weapon_def(W_BAT)->folder, "weapon-bat") == 0);
    CHECK(weapon_def((WeaponId)-1) == NULL);
    CHECK(weapon_def((WeaponId)4) == NULL);
    CHECK(weapon_def((WeaponId)99) == NULL);
}

static GameInput press(uint8_t b) {
    GameInput in = inp(0.0f, 0.0f, false, 0);
    in.pressed = b;
    return in;
}

/* A or B thrusts the weapon out and back, ignores presses mid-jab, and a docked weapon never jabs. */
static void test_jab_out_and_back(void) {
    const WeaponDef* w = weapon_def(W_BAT);
    uint8_t buttons[2] = {BTN_A, BTN_B};
    for (int b = 0; b < 2; b++) {
        Weapon wp;
        start(&wp, w, 0.0f);
        CHECK_NEAR(weapon_jab_ext(&wp), 0.0f, 0.0f);
        GameInput in = press(buttons[b]);
        weapon_update(&wp, &in, w);
        CHECK(weapon_jab_extending(&wp));
        float prev = weapon_jab_ext(&wp), peak = prev;
        CHECK(prev > 0.0f);
        int f;
        for (f = 0; f < WEAPON_JAB_OUT_FRAMES - 1; f++) {
            in = press(buttons[b]);                    /* mashing does not restart it */
            weapon_update(&wp, &in, w);
            CHECK(weapon_jab_ext(&wp) > prev);
            prev = peak = weapon_jab_ext(&wp);
        }
        CHECK_NEAR(peak, WEAPON_JAB_PX, 0.001f);
        CHECK(!weapon_jab_extending(&wp) || wp.jab_t == WEAPON_JAB_OUT_FRAMES);
        for (f = 0; f < 20 && wp.jab_t != 0; f++) crank(&wp, w, 0.0f, 1);
        CHECK_EQ_INT(wp.jab_t, 0);
        CHECK_NEAR(weapon_jab_ext(&wp), 0.0f, 0.0f);
        CHECK_NEAR(wp.angle, 0.0f, 0.001f);           /* jabbing never turns the weapon */
    }
    Weapon wp = {0};
    GameInput d = inp(0.0f, 0.0f, true, 0);
    d.pressed = BTN_A;
    weapon_update(&wp, &d, w);
    CHECK_EQ_INT(wp.jab_t, 0);
}

void suite_weapon_motion(void) {
    test_weapons_table();
    test_zero_is_docked_upright();
    test_undock_teleports();
    test_moves_same_frame();
    test_catch_up_targets();
    test_heavier_lags_more();
    test_sledge_overshoots_slightly();
    test_vel_clamped();
    test_loose_slows_and_stops();
    test_reconnect_turning_back();
    test_reconnect_full_turn_forward();
    test_no_false_reconnect_at_seam();
    test_pivot_shift_eases();
    test_dock_mid_bounce_resets();
    test_long_session_stays_precise();
    test_jab_out_and_back();
}
