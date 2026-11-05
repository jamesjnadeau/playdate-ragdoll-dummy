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
    for (i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g->rigs[i]);
}

void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]) {
    memcpy(g->rigs, rigs, sizeof(g->rigs));
}

void game_start_bench(Game* g, const BenchConfig* cfg) {
    game_init(g, cfg->seed);
    g->player.z = cfg->distance;
    g->cam.player_z = cfg->distance;
    camera_update(&g->cam);
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

void game_update(Game* g, const GameInput* in) {
    if (in->pressed & BTN_UP) camera_set_aim(&g->cam, g->cam.aim + 1);
    if (in->pressed & BTN_DOWN) camera_set_aim(&g->cam, g->cam.aim - 1);
    camera_update(&g->cam);

    player_update(&g->player, in, &g->hall, &g->dummies, g->pending_damage);
    g->pending_damage = 0;
    g->cam.player_z = g->player.z;
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
}

void game_render(Game* g) {
    rl_clear(&g->rl);
    draw_world(&g->rl, &g->cam, &g->hall, &g->dummies, g->rigs, g->frame);
}
