#include <string.h>
#include "game.h"
#include "draw_world.h"
#include "tuning.h"

void game_init(Game* g, uint32_t seed) {
    int i;
    memset(g, 0, sizeof(*g));
    rng_seed(&g->rng, seed);
    camera_set_aim(&g->cam, 1);
    g->cam.pitch_px = 0.0f;
    hallway_init(&g->hall, &g->rng);
    player_init(&g->player);
    g->weapon = W_BAT;
    for (i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g->rigs[i]);
    for (i = 0; i < WEAPON_COUNT; i++) weapon_rig_default(i, &g->wrigs[i]);
}

void game_set_weapon_rigs(Game* g, const WeaponRig wrigs[WEAPON_COUNT]) {
    memcpy(g->wrigs, wrigs, sizeof(g->wrigs));
}

void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]) {
    memcpy(g->rigs, rigs, sizeof(g->rigs));
}

void game_start_bench(Game* g, const BenchConfig* cfg) {
    game_init(g, cfg->seed);
    g->player.z = cfg->distance;
    g->cam.player_z = cfg->distance;
    camera_update(&g->cam);
    if (cfg->scene == BENCH_RAGDOLL) {   /* one dummy flung at the right wall, for screenshots */
        int d = dummy_spawn(&g->dummies, DS_MEDIUM, -0.5f, cfg->distance + 1.8f, -1);
        g->dummies.d[d].state = DST_RAGDOLL;
        g->dummies.d[d].launch = (Vec3){ 4.2f, 2.5f, 0.3f };
    }
}

/* Ticks open doors and spawns their dummies one by one. */
static void spawn_from_doors(Game* g) {
    int i;
    for (i = 0; i < g->hall.ndoors; i++) {
        Door* d = &g->hall.doors[i];
        if (d->state != DOOR_OPEN || d->remaining <= 0) continue;
        if (d->spawn_timer > 0) d->spawn_timer--;
        if (d->spawn_timer > 0) continue;
        if (dummy_spawn(&g->dummies, hallway_pick_size(&g->rng, d->z),
                        (float)d->side * (HALL_HALF_W - DOOR_SPAWN_INSET),
                        d->z + DOOR_LEN * 0.5f, i) < 0)
            continue;   /* pool full: retry next frame */
        d->remaining--;
        d->spawn_timer = DOOR_SPAWN_INTERVAL;
    }
}

/* Adds a star for each hit at the projected head or body point of the struck dummy. */
static void spawn_impacts(Game* g) {
    int i, k;
    for (i = 0; i < g->nhits; i++) {
        const HitResult* h = &g->hits[i];
        const Dummy* d;
        const DummyRig* rig;
        Vec3 w;
        float sx, sy, ppm;
        Impact* slot = &g->impacts[0];
        if (h->dummy < 0 || h->dummy >= MAX_DUMMIES) continue;
        d = &g->dummies.d[h->dummy];
        rig = &g->rigs[d->size];
        w.x = d->x; w.z = d->z;
        w.y = -(h->head ? rig->face_center.y : rig->body_center.y) / rig->height_px * dummy_height(d->size);
        if (!camera_project(&g->cam, w, &sx, &sy, &ppm)) continue;
        for (k = 0; k < MAX_IMPACTS; k++) {   /* a free slot, else recycle the oldest */
            if (g->impacts[k].t <= 0) { slot = &g->impacts[k]; break; }
            if (g->impacts[k].t < slot->t) slot = &g->impacts[k];
        }
        slot->r = clampf(ppm * IMPACT_R_PER_PPM, IMPACT_MIN_R, IMPACT_MAX_R);
        /* A close dummy's body can sit below the screen: keep the whole star visible. */
        slot->x = clampf(sx, slot->r, SCREEN_W - slot->r);
        slot->y = clampf(sy, slot->r, SCREEN_H - slot->r);
        slot->t = IMPACT_FRAMES;
    }
}

void game_update(Game* g, const GameInput* in) {
    int i, pev;
    for (i = 0; i < MAX_IMPACTS; i++) if (g->impacts[i].t > 0) g->impacts[i].t--;
    if (g->hurt_t > 0) g->hurt_t--;
    pev = player_update(&g->player, in, &g->hall, &g->dummies, g->pending_damage);
    if (pev & PEV_HURT) g->hurt_t = HURT_SHAKE_FRAMES;
    g->pending_damage = 0;
    g->cam.player_z = g->player.z;
    if (in->pressed & BTN_UP) camera_set_aim(&g->cam, g->cam.aim + 1);
    if (in->pressed & BTN_DOWN) camera_set_aim(&g->cam, g->cam.aim - 1);
    camera_update(&g->cam);
    g->frame++;

    hallway_update(&g->hall, &g->rng, g->cam.player_z);
    if (g->hall.recycled > 0) {
        int i;
        for (i = 0; i < MAX_DUMMIES; i++) {
            Dummy* d = &g->dummies.d[i];
            if (!d->alive || d->door < 0) continue;
            d->door -= g->hall.recycled;
            if (d->door < 0) d->door = -1;
        }
    }
    spawn_from_doors(g);
    {
        int dmg = 0;
        dummy_update_all(&g->dummies, g->player.z, g->player.x, &dmg);
        g->pending_damage += dmg;
    }

    /* Swing. The crank jump on the frame it leaves the dock must not count as a swing. */
    g->nhits = 0;
    {
        GameInput si = *in;
        const WeaponDef* w = weapon_def(g->weapon);
        if (g->prev_docked && !in->crank_docked) si.crank_delta = 0.0f;
        g->prev_docked = in->crank_docked;
        if (swing_update(&g->swing, &si, w) == SWE_IMPACT)
            g->nhits = combat_resolve(&g->swing, w, g->cam.aim, g->player.z, &g->dummies,
                                      g->hits, (int)(sizeof(g->hits) / sizeof(g->hits[0])));
    }
    spawn_impacts(g);

    /* Knocked-out dummies become ragdolls, then every ragdoll steps once. */
    for (i = 0; i < MAX_DUMMIES; i++) {
        Dummy* d = &g->dummies.d[i];
        if (d->alive && d->state == DST_RAGDOLL && d->ragdoll < 0)
            ragdoll_spawn(&g->ragdolls, &g->dummies, i, &g->rigs[d->size], d->launch);
    }
    {
        RagdollStepOut ro;
        ragdoll_step(&g->ragdolls, &g->dummies, &g->hall, g->rigs, &ro);
        g->nbursts = ro.burst_count;
        for (i = 0; i < ro.burst_count; i++) g->bursts[i] = ro.burst_at[i];
    }
}

void game_render(Game* g) {
    int i;
    rl_clear(&g->rl);
    draw_world(&g->rl, &g->cam, &g->hall, &g->dummies, g->rigs, &g->ragdolls, g->frame);
    if (g->hurt_t > 0) {
        /* Fixed pattern, so renders are deterministic. Only the world shakes. */
        static const int8_t pattern[HURT_SHAKE_FRAMES][2] = { {1, -1}, {-1, 1}, {1, 1}, {-1, -1}, {1, 0}, {0, 1} };
        const int8_t* o = pattern[HURT_SHAKE_FRAMES - g->hurt_t];
        rl_offset(&g->rl, 0, o[0] * HURT_SHAKE_PX, o[1] * HURT_SHAKE_PX);
        g->rl.invert = g->hurt_t == HURT_SHAKE_FRAMES;
    }
    for (i = 0; i < MAX_IMPACTS; i++)
        if (g->impacts[i].t > 0) draw_impact(&g->rl, g->impacts[i].x, g->impacts[i].y, g->impacts[i].r);
    draw_weapon(&g->rl, &g->swing, &g->wrigs[g->weapon], IMG_WEAPON_HELD0 + (int)g->weapon);
}
