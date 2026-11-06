#include <string.h>
#include "test.h"
#include "contact.h"
#include "pose.h"
#include "tuning.h"

#define FRAME 0
/* Scene setups (camera pitch, dummy distance). Each keeps the a*-30 start clear of the boxes for a pivot at
   y 376..400, and the aimed box centre inside the weapon's span (aim_at checks it). */
#define PITCH_LOW (-40.0f)           /* small dummy at DZ_LOW: head out of reach, body in reach */
#define DZ_LOW 1.0f
#define PITCH_MID 0.0f                /* medium dummy at DZ_MID: head out of reach, body in reach */
#define DZ_MID 1.2f
#define PITCH_HIGH 40.0f             /* small dummy at DZ_HIGH: head and body both in the arc */
#define DZ_HIGH 1.2f

static DummyRig g_rigs[DS_COUNT];
static WeaponRig g_wr[W_COUNT];

static void init_rigs(void) {
    int i;
    for (i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g_rigs[i]);
    for (i = 0; i < W_COUNT; i++) weapon_rig_default(i, &g_wr[i]);
}

static Camera mk_cam(float pitch) {
    Camera c;
    memset(&c, 0, sizeof c);
    c.pitch_px = pitch;
    return c;
}

static int put(DummyPool* p, DummySize sz, float x, float z) {
    int i = dummy_spawn(p, sz, x, z, -1);
    p->d[i].state = DST_WALK;
    return i;
}

static void boxes_of(const Camera* cam, const Dummy* d, ScreenBox* head, ScreenBox* body) {
    DummyPose pose;
    bool ok;
    if (d->reaction_t > 0) pose_for_reaction((ReactionType)d->reaction, (float)d->reaction_t / (float)REACTION_FRAMES, d, FRAME, &pose);
    else pose_for_state(d, FRAME, &pose);
    ok = dummy_hit_boxes(cam, d, &g_rigs[d->size], &pose, head, body);
    CHECK(ok);
}

/* Angle that points the weapon at the centre of b; checks the centre is within the weapon's span. */
static float aim_at(const Weapon* wp, const WeaponRig* wr, const ScreenBox* b) {
    float bx = 0.5f * (b->x0 + b->x1), by = 0.5f * (b->y0 + b->y1);
    float dx = bx - weapon_pivot_x(wp), dy = WEAPON_PIVOT_Y - by;
    float dist = sqrtf(dx * dx + dy * dy);
    CHECK(dist >= WEAPON_SWEEP_RADIUS);
    CHECK(dist <= weapon_tip_radius(wr));
    return atan2f(dx, dy) * (180.0f / 3.14159265f);
}

static float aim_body(const Camera* cam, const Dummy* d, const Weapon* wp, const WeaponRig* wr) {
    ScreenBox h, b;
    boxes_of(cam, d, &h, &b);
    return aim_at(wp, wr, &b);
}

static void set_sweep(Weapon* wp, float a0, float a1, float vel) {
    wp->prev_angle = a0;
    wp->angle = a1;
    wp->vel = vel;
    wp->attached = wp->swept = wp->live = true;
}

/* Scene: one dummy at dz, weapon swept a*-30 -> a*+10 at vel. */
static void scene1(DummyPool* p, Weapon* wp, const Camera* cam, WeaponId wid, DummySize sz, float dz, float vel,
                   float* astar) {
    int i;
    memset(p, 0, sizeof *p);
    memset(wp, 0, sizeof *wp);
    i = put(p, sz, 0.0f, dz);
    *astar = aim_body(cam, &p->d[i], wp, &g_wr[wid]);
    set_sweep(wp, *astar - 30.0f, *astar + 10.0f, vel);
}

static int sweep(Weapon* wp, WeaponId wid, const Camera* cam, DummyPool* p, HitResult* h, int max_out) {
    return contact_sweep(wp, weapon_def(wid), &g_wr[wid], cam, p, g_rigs, FRAME, h, max_out);
}

static bool in_box(float x, float y, const ScreenBox* b) {
    return x >= b->x0 - 0.01f && x <= b->x1 + 0.01f && y >= b->y0 - 0.01f && y <= b->y1 + 0.01f;
}

static float tip_m(WeaponId wid, float vel) {
    return contact_momentum(weapon_def(wid), vel, weapon_tip_radius(&g_wr[wid]));
}

static void test_balance_checks(void) {
    CHECK(tip_m(W_CHICKEN, 30.0f) >= dummy_resist(DS_SMALL));
    CHECK(tip_m(W_CHICKEN, 20.0f) < dummy_resist(DS_SMALL));
    CHECK(tip_m(W_CHICKEN, 30.0f) < dummy_resist(DS_MEDIUM));
    CHECK(tip_m(W_BAT, 15.0f) >= dummy_resist(DS_SMALL));
    CHECK(tip_m(W_BAT, 10.0f) < dummy_resist(DS_SMALL));
    CHECK(tip_m(W_SLEDGE, 8.0f) >= dummy_resist(DS_MEDIUM));
    CHECK_NEAR(tip_m(W_CHICKEN, 30.0f), 47.8f, 0.5f);
    CHECK_NEAR(tip_m(W_SLEDGE, 8.0f), 83.1f, 0.5f);
    CHECK_NEAR(tip_m(W_BAT, -15.0f), tip_m(W_BAT, 15.0f), 0.0001f);
}

/* A body contact at the tip at normal speed (20 deg/frame) does the old canned swing's factor-1.0 damage. */
static void test_normal_speed_matches_old_damage(void) {
    static const float old_damage[W_COUNT] = { 0.6f, 1.0f, 1.4f, 2.4f };
    int i;
    for (i = 0; i < W_COUNT; i++) {
        const WeaponDef* w = weapon_def((WeaponId)i);
        float dmg = tip_m((WeaponId)i, 20.0f) * w->power * DAMAGE_PER_MOMENTUM;
        CHECK_NEAR(dmg, old_damage[i], 0.03f * old_damage[i]);
    }
}

/* Heavy sound: the sledge's lagging first hit in the game-flow sound test (m 52.5) is heavy;
   fast bat and pan hits at the tip and the chicken at top speed are light. */
static void test_heavy_sound_rule(void) {
    CHECK(contact_heavy(weapon_def(W_SLEDGE), 52.5f));
    CHECK(!contact_heavy(weapon_def(W_SLEDGE), 30.0f));
    CHECK(!contact_heavy(weapon_def(W_BAT), tip_m(W_BAT, 30.0f)));
    CHECK(!contact_heavy(weapon_def(W_PAN), tip_m(W_PAN, 30.0f)));
    CHECK(!contact_heavy(weapon_def(W_CHICKEN), tip_m(W_CHICKEN, WEAPON_MAX_VEL)));
}

static void test_reaction_from_tangent(void) {
    CHECK_EQ_INT(contact_reaction(0.0f, 20.0f), RX_FROM_LEFT);
    CHECK_EQ_INT(contact_reaction(0.0f, -20.0f), RX_FROM_RIGHT);
    CHECK_EQ_INT(contact_reaction(90.0f, -20.0f), RX_UPWARD);
    CHECK_EQ_INT(contact_reaction(90.0f, 20.0f), RX_DOWNWARD);
}

static void test_sweep_hits_dummy_it_crosses(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a; ScreenBox hb, bb;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 30.0f, &a);
    boxes_of(&cam, &p.d[0], &hb, &bb);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK_EQ_INT(h[0].dummy, 0);
    CHECK(p.d[0].state == DST_STAGGER || p.d[0].state == DST_RAGDOLL);
    CHECK(in_box(h[0].sx, h[0].sy, &hb) || in_box(h[0].sx, h[0].sy, &bb));
    CHECK(h[0].damage > 0.0f);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp - h[0].damage, 0.0001f);
    CHECK_EQ_INT(p.d[0].reaction_t, REACTION_FRAMES);
    CHECK(p.d[0].z > DZ_LOW);
    CHECK(wp.attached);
}

static void test_sweep_misses_dummy_not_crossed(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 30.0f, &a);
    set_sweep(&wp, a + 15.0f, a + 40.0f, 30.0f);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp, 0.0001f);
    CHECK_EQ_INT(p.d[0].state, DST_WALK);
}

static void test_out_of_reach_not_hit(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 30.0f, &a);
    p.d[0].z = weapon_def(W_BAT)->reach + 0.2f;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp, 0.0001f);
}

/* A bat swung slowly is too light to get through: it bounces. */
#define SLOW_VEL 6.0f

static void test_bounce_detaches_and_reverses(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_BAT, DS_MEDIUM, DZ_MID, SLOW_VEL, &a);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK(h[0].damage > 0.0f);
    CHECK(!h[0].knockout);
    CHECK(!wp.attached);
    CHECK(wp.vel < 0.0f);
    CHECK_NEAR(wp.vel, -SLOW_VEL * WEAPON_BOUNCE_RESTITUTION, 0.0001f);
    CHECK(wp.angle > a - 30.0f && wp.angle < a + 10.0f);
}

static void test_penetration_slows(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a, m;
    const WeaponDef* w = weapon_def(W_SLEDGE);
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_SLEDGE, DS_MEDIUM, DZ_MID, 20.0f, &a);
    CHECK_EQ_INT(sweep(&wp, W_SLEDGE, &cam, &p, h, 4), 1);
    CHECK(!h[0].head);
    m = h[0].damage / (w->power * DAMAGE_PER_MOMENTUM);
    CHECK(m >= dummy_resist(DS_MEDIUM));
    CHECK(wp.attached);
    CHECK(fabsf(wp.vel) < 20.0f);
    CHECK_NEAR(wp.vel, 20.0f * (1.0f - dummy_resist(DS_MEDIUM) / m), 0.01f);
}

static void test_second_dummy_after_bounce_untouched(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float al, ar;
    int l, r;
    Camera cam = mk_cam(PITCH_LOW);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    l = put(&p, DS_SMALL, -0.4f, DZ_LOW);
    r = put(&p, DS_SMALL, 0.4f, DZ_LOW);
    al = aim_body(&cam, &p.d[l], &wp, &g_wr[W_BAT]);
    ar = aim_body(&cam, &p.d[r], &wp, &g_wr[W_BAT]);
    set_sweep(&wp, al - 30.0f, ar + 10.0f, SLOW_VEL);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK_EQ_INT(h[0].dummy, l);
    CHECK(!wp.attached);
    CHECK_NEAR(p.d[r].hp, p.d[r].max_hp, 0.0001f);
    CHECK_EQ_INT(p.d[r].state, DST_WALK);
}

/* The rubber chicken is too floppy to bounce: it bends around the dummy and keeps going. */
static void test_chicken_never_bounces(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_MID);
    CHECK(weapon_def(W_CHICKEN)->flexible);
    CHECK(!weapon_def(W_BAT)->flexible && !weapon_def(W_PAN)->flexible && !weapon_def(W_SLEDGE)->flexible);
    CHECK(tip_m(W_CHICKEN, 20.0f) < dummy_resist(DS_MEDIUM));   /* would bounce under the normal rule */
    scene1(&p, &wp, &cam, W_CHICKEN, DS_MEDIUM, DZ_MID, 20.0f, &a);
    CHECK_EQ_INT(sweep(&wp, W_CHICKEN, &cam, &p, h, 4), 1);
    CHECK(h[0].damage > 0.0f);
    CHECK(wp.attached);
    CHECK(wp.vel == 20.0f);
}

static void test_chicken_passes_through_two_dummies(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float al, ar;
    int l, r;
    Camera cam = mk_cam(PITCH_LOW);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    l = put(&p, DS_SMALL, -0.4f, DZ_LOW);
    r = put(&p, DS_SMALL, 0.4f, DZ_LOW);
    al = aim_body(&cam, &p.d[l], &wp, &g_wr[W_CHICKEN]);
    ar = aim_body(&cam, &p.d[r], &wp, &g_wr[W_CHICKEN]);
    set_sweep(&wp, al - 30.0f, ar + 10.0f, 20.0f);
    CHECK_EQ_INT(sweep(&wp, W_CHICKEN, &cam, &p, h, 4), 2);
    CHECK_EQ_INT(h[0].dummy, l);
    CHECK_EQ_INT(h[1].dummy, r);
    CHECK(wp.attached);
    CHECK(wp.vel == 20.0f);
}

static void test_line_of_dummies_saps_momentum(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float al, ar;
    int l, r;
    Camera cam = mk_cam(PITCH_MID);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    l = put(&p, DS_MEDIUM, -0.5f, DZ_MID);
    r = put(&p, DS_MEDIUM, 0.5f, DZ_MID);
    al = aim_body(&cam, &p.d[l], &wp, &g_wr[W_SLEDGE]);
    ar = aim_body(&cam, &p.d[r], &wp, &g_wr[W_SLEDGE]);
    set_sweep(&wp, al - 30.0f, ar + 10.0f, 30.0f);
    CHECK_EQ_INT(sweep(&wp, W_SLEDGE, &cam, &p, h, 4), 2);
    CHECK_EQ_INT(h[0].dummy, l);
    CHECK_EQ_INT(h[1].dummy, r);
    CHECK(!h[0].head && !h[1].head);
    CHECK(h[1].damage < h[0].damage);
    CHECK(wp.attached);
    CHECK(wp.vel > 0.0f && wp.vel < 30.0f);
}

static void test_ko_counts_as_through(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_BAT, DS_MEDIUM, DZ_MID, SLOW_VEL, &a);
    p.d[0].hp = 0.01f;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK(h[0].knockout);
    CHECK_EQ_INT(p.d[0].state, DST_RAGDOLL);
    CHECK(h[0].launch.z > 0.0f);
    CHECK_NEAR(v3_len(h[0].launch), weapon_def(W_BAT)->launch * HIT_SCALE_MIN, 0.01f);
    CHECK(wp.attached);
    CHECK(wp.vel == 0.0f);
}

static void test_head_anywhere_in_pass(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a, dx, dy;
    ScreenBox hb, bb;
    const WeaponDef* w = weapon_def(W_BAT);
    Camera cam = mk_cam(PITCH_HIGH);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    put(&p, DS_SMALL, 0.0f, DZ_HIGH);
    boxes_of(&cam, &p.d[0], &hb, &bb);
    a = aim_at(&wp, &g_wr[W_BAT], &hb);
    CHECK(bb.x1 - bb.x0 > hb.x1 - hb.x0);   /* the wider body is entered before the head */
    set_sweep(&wp, a - 30.0f, a + 10.0f, 20.0f);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK(h[0].head);
    CHECK(in_box(h[0].sx, h[0].sy, &hb));
    dx = h[0].sx - weapon_pivot_x(&wp);
    dy = h[0].sy - WEAPON_PIVOT_Y;
    CHECK_NEAR(h[0].damage,
               contact_momentum(w, 20.0f, sqrtf(dx * dx + dy * dy)) * w->power * HEAD_MULT * DAMAGE_PER_MOMENTUM,
               0.001f);
}

/* A hit that bounces stops at the entry sample, so a head the weapon only reaches later does not count. */
static void test_bounce_in_head_scene_scores_body(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a, a_k, dx, dy;
    ScreenBox hb, bb;
    const WeaponDef* w = weapon_def(W_BAT);
    Camera cam = mk_cam(PITCH_HIGH);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    put(&p, DS_SMALL, 0.0f, DZ_HIGH);
    boxes_of(&cam, &p.d[0], &hb, &bb);
    a = aim_at(&wp, &g_wr[W_BAT], &hb);
    set_sweep(&wp, a - 30.0f, a + 10.0f, SLOW_VEL);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
    CHECK(!wp.attached);                       /* it bounced */
    a_k = wp.angle;                            /* detached at the entry angle */
    CHECK(a_k < a);                            /* entered the body before reaching the head */
    CHECK(!h[0].head);
    CHECK(in_box(h[0].sx, h[0].sy, &bb));
    dx = h[0].sx - weapon_pivot_x(&wp);
    dy = h[0].sy - WEAPON_PIVOT_Y;
    CHECK_NEAR(h[0].damage, contact_momentum(w, SLOW_VEL, sqrtf(dx * dx + dy * dy)) * w->power * DAMAGE_PER_MOMENTUM,
               0.001f);
}

/* Normal HP: a sledge at normal speed takes a medium dummy (3 HP) in two hits (spec decision 10), so it
   survives the first and stays a target even though the push-back shrinks its boxes off the weapon. */
static void test_one_hit_per_pass(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    int total = 0;
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_SLEDGE, DS_MEDIUM, DZ_MID, 20.0f, &a);
    set_sweep(&wp, a - 30.0f, a, 20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 1);
    CHECK(!h[0].knockout);
    CHECK(wp.attached);
    CHECK(p.d[0].z - cam.player_z <= weapon_def(W_SLEDGE)->reach);   /* pushed back, still in reach */
    CHECK(wp.inside & 1u);           /* still counted as under the weapon */
    set_sweep(&wp, a, a + 3.0f, 20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 1);
    /* Leave every box, then come back. */
    set_sweep(&wp, a + 3.0f, a + 40.0f, 20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 1);
    CHECK(!(wp.inside & 1u));
    set_sweep(&wp, a + 40.0f, a - 10.0f, -20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 2);
    CHECK(h[0].knockout);            /* the second hit of the second pass */
}

/* A whole crank-driven stroke (the spring, the slow-down on penetration, the push-back and reaction pose):
   a sledge at normal speed hits a medium dummy once, and it survives. */
static void test_full_stroke_hits_once(void) {
    DummyPool p; Weapon wp; HitResult h[4]; GameInput in; float a;
    int f, total = 0;
    const WeaponDef* w = weapon_def(W_SLEDGE);
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_SLEDGE, DS_MEDIUM, DZ_MID, 20.0f, &a);
    memset(&wp, 0, sizeof wp);
    memset(&in, 0, sizeof in);
    in.crank_angle = a - 60.0f + CRANK_ZERO_DEG;
    weapon_update(&wp, &in, w);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    in.crank_delta = 20.0f;
    for (f = 0; f < 20; f++) {
        if (f == 8) in.crank_delta = 0.0f;
        in.crank_angle += in.crank_delta;
        weapon_update(&wp, &in, w);
        total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    }
    CHECK(wp.angle > a + 60.0f);   /* the stroke carried on well past the dummy */
    CHECK_EQ_INT(total, 1);
    CHECK(p.d[0].hp > 0.0f);
    CHECK(p.d[0].state != DST_RAGDOLL);
}

/* The struck dummy is let go by a return stroke, and by the next crank turn in the same direction. */
static void test_struck_dummy_released_by_return_and_next_turn(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a, x;
    int total = 0;
    Camera cam = mk_cam(PITCH_MID);
    scene1(&p, &wp, &cam, W_SLEDGE, DS_MEDIUM, DZ_MID, 20.0f, &a);
    set_sweep(&wp, a - 30.0f, a + 40.0f, 20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 1);
    CHECK(wp.inside & 1u);           /* struck: counts as inside though the weapon is past it */
    set_sweep(&wp, a + 40.0f, a - 30.0f, -20.0f);
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 2);          /* the return stroke is a new pass */
    CHECK(h[0].knockout);

    scene1(&p, &wp, &cam, W_SLEDGE, DS_MEDIUM, DZ_MID, 20.0f, &a);
    set_sweep(&wp, a - 30.0f, a + 40.0f, 20.0f);
    total = sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    for (x = a + 40.0f; x < a + 320.0f; x += 20.0f) {
        set_sweep(&wp, x, x + 20.0f, 20.0f);
        total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    }
    CHECK_EQ_INT(total, 1);
    CHECK(!(wp.inside & 1u));
    set_sweep(&wp, a + 320.0f, a + 370.0f, 20.0f);   /* round again to the dummy */
    total += sweep(&wp, W_SLEDGE, &cam, &p, h, 4);
    CHECK_EQ_INT(total, 2);
}

static void test_still_weapon_never_hits(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 30.0f, &a);
    /* Not swept (teleport / docked / loose) onto the dummy. */
    set_sweep(&wp, a - 30.0f, a, 30.0f);
    wp.swept = false;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK(wp.inside & 1u);
    /* A swept frame that starts inside. */
    set_sweep(&wp, a, a + 4.0f, 30.0f);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp, 0.0001f);
    /* Shift-only: off the dummy, then slide sideways onto it without turning. */
    set_sweep(&wp, a + 20.0f, a + 20.0f, 0.0f);
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK(!(wp.inside & 1u));
    wp.shift = -70.0f;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK(wp.inside & 1u);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp, 0.0001f);
    CHECK_EQ_INT(p.d[0].state, DST_WALK);
}

static void test_max_out_respected(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float al, ar;
    int i, l, r;
    Camera cam = mk_cam(PITCH_LOW);
    memset(&p, 0, sizeof p);
    memset(&wp, 0, sizeof wp);
    l = put(&p, DS_SMALL, -0.4f, DZ_LOW);
    put(&p, DS_SMALL, 0.0f, DZ_LOW);
    r = put(&p, DS_SMALL, 0.4f, DZ_LOW);
    al = aim_body(&cam, &p.d[l], &wp, &g_wr[W_SLEDGE]);
    ar = aim_body(&cam, &p.d[r], &wp, &g_wr[W_SLEDGE]);
    set_sweep(&wp, al - 30.0f, ar + 10.0f, 30.0f);
    h[1].dummy = -7;
    CHECK_EQ_INT(sweep(&wp, W_SLEDGE, &cam, &p, h, 1), 1);
    CHECK_EQ_INT(h[0].dummy, l);
    CHECK_EQ_INT(h[1].dummy, -7);              /* nothing written past max_out */
    for (i = 0; i < 3; i++) {
        if (i == l) continue;
        CHECK_NEAR(p.d[i].hp, p.d[i].max_hp, 0.0001f);
        CHECK_EQ_INT(p.d[i].state, DST_WALK);
    }
    /* With room, all three are hit. */
    memset(&p, 0, sizeof p);
    put(&p, DS_SMALL, -0.4f, DZ_LOW);
    put(&p, DS_SMALL, 0.0f, DZ_LOW);
    put(&p, DS_SMALL, 0.4f, DZ_LOW);
    memset(&wp, 0, sizeof wp);
    set_sweep(&wp, al - 30.0f, ar + 10.0f, 30.0f);
    CHECK_EQ_INT(sweep(&wp, W_SLEDGE, &cam, &p, h, 4), 3);
}

/* A still weapon resting on a dummy pokes it once per jab, pushing along itself, and stays attached. */
static void test_jab_hits_once_while_still(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    int f, total = 0;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 0.0f, &a);
    set_sweep(&wp, a, a, 0.0f);
    wp.swept = false;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);   /* resting on it does nothing */
    wp.jab_t = 1;
    for (f = 0; f < WEAPON_JAB_FRAMES; f++) {
        total += sweep(&wp, W_BAT, &cam, &p, h + (total < 4 ? total : 3), 4 - (total < 4 ? total : 3));
        wp.jab_t++;
    }
    CHECK_EQ_INT(total, 1);
    CHECK_EQ_INT(h[0].dummy, 0);
    CHECK(p.d[0].hp < p.d[0].max_hp);
    CHECK(wp.attached);
    CHECK_NEAR(wp.vel, 0.0f, 0.0f);
    /* A second jab hits again. */
    wp.jab_t = 1;
    wp.jab_hit = 0;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 1);
}

/* Retracting never hits; a jab pointing away from every dummy hits nothing. */
static void test_jab_only_hits_going_out(void) {
    DummyPool p; Weapon wp; HitResult h[4]; float a;
    Camera cam = mk_cam(PITCH_LOW);
    scene1(&p, &wp, &cam, W_BAT, DS_SMALL, DZ_LOW, 0.0f, &a);
    set_sweep(&wp, a, a, 0.0f);
    wp.swept = false;
    wp.jab_t = WEAPON_JAB_OUT_FRAMES + 1;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    set_sweep(&wp, a + 60.0f, a + 60.0f, 0.0f);
    wp.swept = false;
    wp.jab_t = 1;
    CHECK_EQ_INT(sweep(&wp, W_BAT, &cam, &p, h, 4), 0);
    CHECK_NEAR(p.d[0].hp, p.d[0].max_hp, 0.0001f);
}

void suite_contact(void) {
    init_rigs();
    test_balance_checks();
    test_normal_speed_matches_old_damage();
    test_heavy_sound_rule();
    test_reaction_from_tangent();
    test_sweep_hits_dummy_it_crosses();
    test_sweep_misses_dummy_not_crossed();
    test_out_of_reach_not_hit();
    test_bounce_detaches_and_reverses();
    test_penetration_slows();
    test_second_dummy_after_bounce_untouched();
    test_chicken_never_bounces();
    test_chicken_passes_through_two_dummies();
    test_line_of_dummies_saps_momentum();
    test_ko_counts_as_through();
    test_head_anywhere_in_pass();
    test_bounce_in_head_scene_scores_body();
    test_one_hit_per_pass();
    test_full_stroke_hits_once();
    test_struck_dummy_released_by_return_and_next_turn();
    test_still_weapon_never_hits();
    test_max_out_respected();
    test_jab_hits_once_while_still();
    test_jab_only_hits_going_out();
}
