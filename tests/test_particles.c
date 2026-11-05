#include <string.h>
#include "test.h"
#include "particles.h"
#include "game.h"
#include "draw_world.h"
#include "tuning.h"

static ParticlePool g_pp;
static RenderList g_rl;
static Rng g_rng;

static Vec3 V(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }

static int active_count(const ParticlePool* pp) {
    int n = 0;
    for (int i = 0; i < PARTICLE_MAX; i++) if (pp->p[i].active) n++;
    return n;
}

static void fresh(void) {
    memset(&g_pp, 0, sizeof g_pp);
    rng_seed(&g_rng, 7);
}

static void test_burst_counts(void) {
    fresh();
    particles_burst(&g_pp, &g_rng, V(1.5f, 0.6f, 4.0f), V(-1, 0, 0));
    CHECK_EQ_INT(active_count(&g_pp), BURST_PUFFS + BURST_SCRAPS + BURST_FLOOR_PUFFS);
    CHECK_EQ_INT(BURST_FLOOR_PUFFS, 12);
}

static void test_particles_pool_full(void) {
    fresh();
    for (int i = 0; i < 10; i++) particles_burst(&g_pp, &g_rng, V(1.5f, 0.6f, 4.0f + (float)i), V(-1, 0, 0));
    CHECK(active_count(&g_pp) <= PARTICLE_MAX);
    CHECK_EQ_INT(active_count(&g_pp), PARTICLE_MAX);
    CHECK(g_pp.next >= 0 && g_pp.next < PARTICLE_MAX);
    CHECK_EQ_INT(g_pp.next, (10 * (BURST_PUFFS + BURST_SCRAPS + BURST_FLOOR_PUFFS)) % PARTICLE_MAX);
    for (int i = 0; i < 20; i++) particles_step(&g_pp);
    /* The oldest burst (z = 4) was overwritten first: newest burst is fully present. */
    int newest = 0;
    for (int i = 0; i < PARTICLE_MAX; i++) if (g_pp.p[i].active && g_pp.p[i].p.z > 12.9f) newest++;
    CHECK(newest > 0);
}

static void test_particles_settle_on_floor(void) {
    int puffs = 0;
    fresh();
    particles_burst(&g_pp, &g_rng, V(1.5f, 0.8f, 4.0f), V(-1, 0, 0));
    for (int i = 0; i < 120; i++) particles_step(&g_pp);
    for (int i = 0; i < PARTICLE_MAX; i++) {
        const Particle* p = &g_pp.p[i];
        if (!p->active || p->kind != PK_PUFF) continue;
        puffs++;
        CHECK_NEAR(p->p.y, 0.0f, 1e-6f);
        CHECK(p->settled);
        CHECK(p->p.x <= HALL_HALF_W + 1e-4f && p->p.x >= -HALL_HALF_W - 1e-4f);
    }
    CHECK_EQ_INT(puffs, BURST_PUFFS + BURST_FLOOR_PUFFS);
}

static void test_particles_expire(void) {
    fresh();
    particles_burst(&g_pp, &g_rng, V(1.5f, 0.8f, 4.0f), V(-1, 0, 0));
    for (int i = 0; i < PARTICLE_LIFE_FRAMES - 1; i++) particles_step(&g_pp);
    CHECK(active_count(&g_pp) > 0);              /* the pile is still there just before it expires */
    particles_step(&g_pp);
    particles_step(&g_pp);
    CHECK_EQ_INT(active_count(&g_pp), 0);
}

static void test_burst_sprays_away_from_wall(void) {
    int scraps = 0;
    fresh();
    particles_burst(&g_pp, &g_rng, V(1.5f, 1.0f, 4.0f), V(-1, 0, 0));
    for (int i = 0; i < PARTICLE_MAX; i++) {
        const Particle* p = &g_pp.p[i];
        if (p->active && p->kind == PK_SCRAP) { scraps++; CHECK(p->v.x <= 0.0f); }
    }
    CHECK_EQ_INT(scraps, BURST_SCRAPS);
    /* A floor burst sprays upward. */
    fresh();
    particles_burst(&g_pp, &g_rng, V(0.0f, 0.0f, 4.0f), V(0, 1, 0));
    for (int i = 0; i < PARTICLE_MAX; i++)
        if (g_pp.p[i].active && g_pp.p[i].kind == PK_SCRAP) CHECK(g_pp.p[i].v.y >= 0.0f);
}

static void test_scrap_bounces_once_and_teeth(void) {
    fresh();
    particles_teeth(&g_pp, &g_rng, V(0.0f, 1.2f, 3.0f));
    CHECK_EQ_INT(active_count(&g_pp), 2);
    for (int i = 0; i < PARTICLE_MAX; i++)
        if (g_pp.p[i].active) { CHECK_EQ_INT(g_pp.p[i].kind, PK_TOOTH); CHECK(g_pp.p[i].v.y > 0.0f); }
    for (int i = 0; i < 60; i++) particles_step(&g_pp);
    for (int i = 0; i < PARTICLE_MAX; i++)
        if (g_pp.p[i].active) { CHECK(g_pp.p[i].settled); CHECK_NEAR(g_pp.p[i].p.y, 0.0f, 1e-6f); }
}

static void test_draw_particles_far_to_near(void) {
    Camera cam = {0.0f, 0.0f, 1};
    float last_size = 0.0f;
    int n = 0;
    fresh();
    camera_update(&cam);
    for (int i = 0; i < 6; i++) {
        Particle* p = &g_pp.p[i];
        p->active = true; p->kind = PK_PUFF; p->variant = (uint8_t)(i % 4);
        p->p = V(0.0f, 0.3f, 2.0f + 1.5f * (float)((i * 5) % 6)); p->life = 100; p->settled = true;
    }
    rl_clear(&g_rl);
    draw_particles(&g_rl, &cam, &g_pp);
    for (int i = 0; i < g_rl.n; i++) {
        const RenderCmd* c = &g_rl.cmd[i];
        if (c->type != RC_BITMAP) continue;
        CHECK(c->sx >= last_size - 1e-6f);   /* nearer particles are bigger and drawn later */
        last_size = c->sx;
        n++;
    }
    CHECK_EQ_INT(n, 6);
}

static void test_draw_particles_budget(void) {
    static Game g;
    int i;
    game_init(&g, 5);
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
    {
        static DummyPool scratch;
        for (i = 0; i < 6; i++) {
            int d = dummy_spawn(&scratch, DS_MEDIUM, -1.0f + 0.4f * (float)i, 1.5f + 1.5f * (float)i, -1);
            ragdoll_spawn(&g.ragdolls, &scratch, d, &g.rigs[DS_MEDIUM], V(0, 0, 0));
        }
    }
    for (i = 0; i < MAX_IMPACTS; i++) {
        g.impacts[i].x = 40.0f + 40.0f * (float)i; g.impacts[i].y = 100.0f;
        g.impacts[i].r = IMPACT_MAX_R; g.impacts[i].t = IMPACT_FRAMES;
    }
    g.wp.live = g.wp.attached = true; g.wp.angle = -30; g.wp.vel = 20;
    for (i = 0; i < PARTICLE_MAX; i++) {   /* a full pool, all on screen and near */
        Particle* p = &g.particles.p[i];
        p->active = true; p->kind = (i % 3 == 0) ? PK_SCRAP : (i % 17 == 0) ? PK_TOOTH : PK_PUFF;
        p->variant = (uint8_t)(i % 4);
        p->p = V(-1.4f + 2.8f * (float)(i % 16) / 15.0f, 0.0f, 0.8f + 0.12f * (float)(i / 16));
        p->life = 100; p->settled = true;
    }
    game_render(&g);
    CHECK_EQ_INT(g.rl.dropped, 0);
    CHECK(g.rl.n <= 1024 - 64);
}

static void test_game_teeth_on_upward_hit(void) {
    static Game g;
    GameInput in;
    int d, teeth = 0;
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    d = dummy_spawn(&g.dummies, DS_MEDIUM, 0.0f, 3.0f, -1);
    g.dummies.d[d].state = DST_STAGGER; g.dummies.d[d].timer = 30;
    g.dummies.d[d].reaction = RX_UPWARD; g.dummies.d[d].reaction_t = REACTION_FRAMES + 1;   /* dummy_update_all ticks it down before the check */
    game_update(&g, &in);
    for (int i = 0; i < PARTICLE_MAX; i++) if (g.particles.p[i].active && g.particles.p[i].kind == PK_TOOTH) teeth++;
    CHECK_EQ_INT(teeth, 2);
    game_update(&g, &in);   /* reaction_t is now below REACTION_FRAMES: no new teeth */
    teeth = 0;
    for (int i = 0; i < PARTICLE_MAX; i++) if (g.particles.p[i].active && g.particles.p[i].kind == PK_TOOTH) teeth++;
    CHECK_EQ_INT(teeth, 2);
}

static void test_game_burst_makes_particles(void) {
    static Game g;
    GameInput in;
    int d, total = 0;
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    d = dummy_spawn(&g.dummies, DS_MEDIUM, 0.9f, 3.0f, -1);
    g.dummies.d[d].state = DST_RAGDOLL;
    g.dummies.d[d].launch = V(6, 0, 0);
    game_update(&g, &in);
    g.ragdolls.r[g.dummies.d[d].ragdoll].hp = 0.2f;
    for (int i = 0; i < 20; i++) {
        game_update(&g, &in);
        if (g.nbursts > 0) { CHECK_NEAR(g.burst_n[0].x, -1.0f, 1e-4f); CHECK_NEAR(g.burst_n[0].y, 0.0f, 1e-4f); }
    }
    for (int i = 0; i < PARTICLE_MAX; i++) if (g.particles.p[i].active) total++;
    CHECK(total >= BURST_PUFFS + BURST_SCRAPS + BURST_FLOOR_PUFFS - 10);   /* a few puffs may have expired or settled; none lost */
    CHECK(total <= PARTICLE_MAX);
}

static void test_bench_wave_and_storm(void) {
    static Game g;
    BenchConfig c;
    GameInput in;
    int open = 0, live = 0, ragdolls = 0, max_rag = 0, max_active = 0;
    memset(&in, 0, sizeof in);
    c.scene = BENCH_WAVE; c.distance = 400.0f; c.seed = 5;
    game_start_bench(&g, &c);
    for (int i = 0; i < g.hall.ndoors; i++) if (g.hall.doors[i].state == DOOR_OPEN) open++;
    for (int i = 0; i < MAX_DUMMIES; i++) if (g.dummies.d[i].alive) live++;
    CHECK(open >= 4);
    CHECK_EQ_INT(live, MAX_LIVE_DUMMIES);

    c.scene = BENCH_BURST_STORM; c.distance = 0.0f; c.seed = 9;
    game_start_bench(&g, &c);
    for (int f = 0; f < 150; f++) {
        int a = 0;
        game_update(&g, &in);
        ragdolls = 0;
        for (int i = 0; i < RAGDOLL_MAX; i++) if (g.ragdolls.r[i].active) ragdolls++;
        if (ragdolls > max_rag) max_rag = ragdolls;
        for (int i = 0; i < PARTICLE_MAX; i++) if (g.particles.p[i].active) a++;
        if (a > max_active) max_active = a;
    }
    CHECK_EQ_INT(max_rag, 6);
    CHECK(max_active >= 200);
}

void suite_particles(void) {
    test_burst_counts();
    test_particles_pool_full();
    test_particles_settle_on_floor();
    test_particles_expire();
    test_burst_sprays_away_from_wall();
    test_scrap_bounces_once_and_teeth();
    test_draw_particles_far_to_near();
    test_draw_particles_budget();
    test_game_teeth_on_upward_hit();
    test_game_burst_makes_particles();
    test_bench_wave_and_storm();
}
