#include <string.h>
#include "game.h"
#include "draw_world.h"
#include "hud.h"
#include <math.h>
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
    g->mode = MODE_PLAY;
    g->offer = W_BAT;
    g->pickup_z = -1.0f;
    for (i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g->rigs[i]);
    for (i = 0; i < WEAPON_COUNT; i++) weapon_rig_default(i, &g->wrigs[i]);
}

/* A fresh run in MODE_PLAY. The bests and the platform-measured rigs belong to the device, not the run. */
void game_start_run(Game* g, uint32_t seed) {
    int best_bags = g->best_bags;
    float best_distance = g->best_distance;
    DummyRig rigs[DS_COUNT];
    WeaponRig wrigs[WEAPON_COUNT];
    memcpy(rigs, g->rigs, sizeof(rigs));
    memcpy(wrigs, g->wrigs, sizeof(wrigs));
    game_init(g, seed);
    memcpy(g->rigs, rigs, sizeof(rigs));
    memcpy(g->wrigs, wrigs, sizeof(wrigs));
    g->best_bags = best_bags;
    g->best_distance = best_distance;
}

void game_show_title(Game* g) {
    int d;
    game_start_run(g, rng_u32(&g->rng) ^ ((uint32_t)g->frame * 2654435761u));
    g->mode = MODE_TITLE;
    hallway_update(&g->hall, &g->rng, 0.0f);
    d = dummy_spawn(&g->dummies, DS_MEDIUM, 0.0f, TITLE_DUMMY_Z, -1);
    if (d >= 0) g->dummies.d[d].state = DST_WALK;
}

void game_push_sound(Game* g, SoundId id) {
    int i;
    for (i = 0; i < g->nsounds; i++) if (g->sounds[i] == (uint8_t)id) return;   /* one player per sound: repeats in a frame are inaudible */
    if (g->nsounds < SOUND_QUEUE_MAX) g->sounds[g->nsounds++] = (uint8_t)id;
}

void game_knock_out(Game* g, int dummy, Vec3 launch) {
    Dummy* d;
    if (dummy < 0 || dummy >= MAX_DUMMIES) return;
    d = &g->dummies.d[dummy];
    if (!d->alive) return;
    d->state = DST_RAGDOLL;
    d->timer = 0;
    d->launch = launch;
    if (!d->scored) { d->scored = true; g->bags++; }
}

void game_apply_bests(Game* g, int best_bags, int best_distance_m) {
    g->best_bags = best_bags < 0 ? 0 : best_bags;
    g->best_distance = best_distance_m < 0 ? 0.0f : (float)best_distance_m;
}

void game_set_weapon_rigs(Game* g, const WeaponRig wrigs[WEAPON_COUNT]) {
    memcpy(g->wrigs, wrigs, sizeof(g->wrigs));
}

void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]) {
    memcpy(g->rigs, rigs, sizeof(g->rigs));
}

/* The run just ended: a new best bag count or distance is kept and flagged for saving. */
static void handle_death(Game* g) {
    g->death_handled = true;
    if (g->bags > g->best_bags) { g->best_bags = g->bags; g->seen_new_best = 1; }
    if ((int)g->player.z > (int)g->best_distance) { g->best_distance = (float)(int)g->player.z; g->seen_new_best = 1; }
    if (g->seen_new_best) g->save_dirty = true;
}

/* Wave: the 4 nearest doors ahead are open with their full quota, and the dummy pool is pre-filled. */
static void bench_wave(Game* g, float distance) {
    int i, opened = 0, k = 0, guard = 0;
    int door_of[4];
    hallway_update(&g->hall, &g->rng, distance);
    for (i = 0; i < g->hall.ndoors && opened < 4; i++) {
        Door* d = &g->hall.doors[i];
        if (d->z < distance) continue;
        d->state = DOOR_OPEN; d->leaf_angle = 1.0f; d->spawn_timer = DOOR_SPAWN_INTERVAL;
        d->remaining = hallway_dummies_for_door(distance);
        door_of[opened++] = i;
    }
    while (opened > 0 && guard++ < MAX_DUMMIES && dummy_live_count(&g->dummies) < MAX_LIVE_DUMMIES) {
        const Door* d = &g->hall.doors[door_of[k % opened]];
        int s = dummy_spawn(&g->dummies, hallway_pick_size(&g->rng, d->z),
                            (float)d->side * (HALL_HALF_W - DOOR_SPAWN_INSET - 0.15f * (float)(k / opened)),
                            d->z + DOOR_LEN * 0.5f + 0.3f * (float)(k / opened), door_of[k % opened]);
        if (s < 0) break;
        g->dummies.d[s].state = DST_WALK;
        k++;
    }
}

/* Burst storm: every BENCH_STORM_PERIOD frames, small dummies are knocked out toward the walls. */
static void bench_storm_tick(Game* g) {
    int i;
    for (i = 0; i < g->hall.ndoors; i++) g->hall.doors[i].remaining = 0;   /* only the storm's dummies */
    if (g->frame % BENCH_STORM_PERIOD != 0) return;
    for (i = 0; i < BENCH_STORM_COUNT; i++) {
        float side = (i & 1) ? -1.0f : 1.0f;
        int d = dummy_spawn(&g->dummies, DS_SMALL, side * 0.3f, g->player.z + 1.6f + 0.35f * (float)i, -1);
        if (d < 0) continue;
        game_knock_out(g, d, (Vec3){ side * 12.0f, 3.0f, 4.0f });
    }
}

void game_start_bench(Game* g, const BenchConfig* cfg) {
    game_init(g, cfg->seed);
    g->player.z = cfg->distance;
    g->cam.player_z = cfg->distance;
    camera_update(&g->cam);
    g->bench = cfg->scene;
    if (cfg->scene == BENCH_WAVE) bench_wave(g, cfg->distance);
    if (cfg->scene == BENCH_OFFER) {   /* a pickup under the player's feet: the first update opens the offer */
        g->weapon = W_BAT;
        g->offer = W_SLEDGE;
        g->pickup_z = cfg->distance + 0.1f;
    }
    if (cfg->scene == BENCH_RAGDOLL) {   /* one dummy flung at the right wall, for screenshots */
        int d = dummy_spawn(&g->dummies, DS_MEDIUM, -0.5f, cfg->distance + 1.8f, -1);
        game_knock_out(g, d, (Vec3){ 4.2f, 2.5f, 0.3f });
    }
    if (cfg->scene == BENCH_TITLE) game_show_title(g);
    if (cfg->scene == BENCH_GAMEOVER) {   /* a finished run with a new best, for screenshots */
        g->bags = 14;
        g->player.z = 120.0f;
        g->player.hearts = 0;
        g->player.dead = true;
        handle_death(g);
        g->mode = MODE_GAME_OVER;
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
        if (dummy_live_count(&g->dummies) >= MAX_LIVE_DUMMIES) continue;   /* cap reached: wait, like pool full */
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

/* Teeth fly on the frame an upward reaction begins, from the head. */
static void spawn_teeth(Game* g) {
    int i;
    for (i = 0; i < MAX_DUMMIES; i++) {
        const Dummy* d = &g->dummies.d[i];
        const DummyRig* rig = &g->rigs[d->size];
        Vec3 at;
        if (!d->alive || d->state == DST_RAGDOLL || d->state == DST_DEAD) continue;
        if (d->reaction_t != REACTION_FRAMES) continue;
        if (d->reaction != RX_UPWARD && d->reaction != RX_LEFT_UP && d->reaction != RX_RIGHT_UP) continue;
        at.x = d->x; at.z = d->z;
        at.y = -rig->face_center.y / rig->height_px * dummy_height(d->size);
        particles_teeth(&g->particles, &g->rng, at);
    }
}

/* Every WEAPON_OFFER_EVERY_DOORS emptied doors, drops a pickup ahead of the player offering another weapon. */
static void maybe_place_pickup(Game* g) {
    int now = g->hall.doors_emptied;
    /* A multiple of WEAPON_OFFER_EVERY_DOORS crossed while a pickup is still pending is intentionally
       skipped (no second pickup, no re-roll): the pending one stays. */
    if (now / WEAPON_OFFER_EVERY_DOORS > g->offer_doors_seen / WEAPON_OFFER_EVERY_DOORS && g->pickup_z < 0.0f) {
        g->pickup_z = g->player.z + PICKUP_AHEAD;
        g->offer = (WeaponId)(((int)g->weapon + 1 + rng_range(&g->rng, 0, (int)W_COUNT - 2)) % (int)W_COUNT);
    }
    g->offer_doors_seen = now;
}

/* The world is paused: A takes the offered weapon, B keeps the current one. Presses only, not holds. */
static void offer_update(Game* g, const GameInput* in) {
    if (!(in->pressed & (BTN_A | BTN_B))) return;
    if (in->pressed & BTN_A) {
        g->weapon = g->offer;
        memset(&g->swing, 0, sizeof(g->swing));   /* a swing in progress belonged to the old weapon */
    }
    g->pickup_z = -1.0f;
    g->mode = MODE_PLAY;
}

/* The title: the hallway idles. A starts a run, B opens the skins menu. */
static void title_update(Game* g, const GameInput* in) {
    g->frame++;
    hallway_update(&g->hall, &g->rng, 0.0f);
    if (in->pressed & BTN_A) game_start_run(g, rng_u32(&g->rng) ^ ((uint32_t)g->frame * 2654435761u));
    else if (in->pressed & BTN_B) g->mode = MODE_SKINS;
}

/* The run is over: A returns to the title, but only after a short lock. */
static void game_over_update(Game* g, const GameInput* in) {
    g->over_t++;
    if (g->over_t > GAME_OVER_LOCK_FRAMES && (in->pressed & BTN_A)) game_show_title(g);
}

void game_update(Game* g, const GameInput* in) {
    int i, pev;
    float prev_z;
    g->nsounds = 0;
    g->held = in->held;
    g->crank_docked = in->crank_docked;
    if (g->mode == MODE_TITLE) { title_update(g, in); return; }
    if (g->mode == MODE_GAME_OVER) { game_over_update(g, in); return; }
    if (g->mode == MODE_OFFER) { offer_update(g, in); return; }
    if (g->mode != MODE_PLAY) return;   /* the skins menu and credits come in later tasks */
    for (i = 0; i < MAX_IMPACTS; i++) if (g->impacts[i].t > 0) g->impacts[i].t--;
    if (g->hurt_t > 0) g->hurt_t--;
    prev_z = g->player.z;
    pev = player_update(&g->player, in, &g->hall, &g->dummies, g->pending_damage);
    if (pev & PEV_HURT) g->hurt_t = HURT_SHAKE_FRAMES;
    if (pev & PEV_DOOR_SLAM) game_push_sound(g, SND_DOOR_SLAM);
    if ((in->held & BTN_A) && g->player.z > prev_z) {
        if (++g->step_t >= STEP_SOUND_FRAMES) { g->step_t = 0; game_push_sound(g, SND_STEP); }
    } else {
        g->step_t = 0;
    }
    g->pending_damage = 0;
    g->cam.player_z = g->player.z;
    maybe_place_pickup(g);
    if (g->pickup_z >= 0.0f && fabsf(g->pickup_z - g->player.z) <= PICKUP_RADIUS) g->mode = MODE_OFFER;
    if (in->pressed & BTN_UP) camera_set_aim(&g->cam, g->cam.aim + 1);
    if (in->pressed & BTN_DOWN) camera_set_aim(&g->cam, g->cam.aim - 1);
    camera_update(&g->cam);
    g->frame++;

    hallway_update(&g->hall, &g->rng, g->cam.player_z);
    if (g->bench == BENCH_BURST_STORM) bench_storm_tick(g);
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
        SwingEvent se = swing_update(&g->swing, &si, w);
        if (se == SWE_STARTED) game_push_sound(g, SND_SWING);
        if (se == SWE_IMPACT) {
            g->nhits = combat_resolve(&g->swing, w, g->cam.aim, g->player.z, &g->dummies,
                                      g->hits, (int)(sizeof(g->hits) / sizeof(g->hits[0])));
            for (i = 0; i < g->nhits; i++) {
                const HitResult* h = &g->hits[i];
                game_push_sound(g, h->damage >= HIT_HEAVY_DAMAGE ? SND_HIT_HEAVY : SND_HIT_LIGHT);
                if (h->knockout) {   /* scored the moment the dummy goes down */
                    Dummy* d = &g->dummies.d[h->dummy];
                    if (!d->scored) { d->scored = true; g->bags++; }
                    game_push_sound(g, SND_GRUNT);
                }
            }
        }
    }
    spawn_impacts(g);
    spawn_teeth(g);

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
        g->bags += ro.knockouts + ro.burst_count;   /* knockouts were marked scored as they happened; a burst is one more bag, once per ragdoll */
        if (ro.knockouts > 0) game_push_sound(g, SND_GRUNT);
        if (ro.door_slams > 0) game_push_sound(g, SND_DOOR_SLAM);
        for (i = 0; i < ro.burst_count; i++) {
            game_push_sound(g, SND_BURST);
            g->bursts[i] = ro.burst_at[i];
            g->burst_n[i] = ro.burst_n[i];
            particles_burst(&g->particles, &g->rng, ro.burst_at[i], ro.burst_n[i]);
        }
    }
    particles_step(&g->particles);

    /* Death is handled last, so the bags of this very frame count towards the bests. */
    if (g->player.hearts <= 0) g->player.dead = true;
    if (g->player.dead && !g->death_handled) {
        handle_death(g);
        if (g->bench == BENCH_NONE) {   /* a benchmark scene keeps running, so it measures what it was built to */
            g->mode = MODE_GAME_OVER;
            g->over_t = 0;
            g->hurt_t = 0;   /* no frozen shake or inverted frame on the game-over screen */
        }
    }
}

void game_render(Game* g) {
    int i;
    rl_clear(&g->rl);
    if (g->mode == MODE_TITLE) {   /* the hallway and one idle dummy, with the menu on plates over it */
        draw_world(&g->rl, &g->cam, &g->hall, &g->dummies, g->rigs, NULL, g->frame);
        rl_hud_begin(&g->rl);
        hud_draw_title(&g->rl, g);
        return;
    }
    draw_world(&g->rl, &g->cam, &g->hall, &g->dummies, g->rigs, &g->ragdolls, g->frame);
    if (g->pickup_z >= 0.0f) {
        int start = g->rl.n;
        draw_pickup(&g->rl, &g->cam, g->pickup_z, IMG_WEAPON_ICON0 + (int)g->offer);
        rl_offset(&g->rl, start, 0, (int)lroundf(PICKUP_BOB_PX * sinf(PICKUP_BOB_RAD_PER_FRAME * (float)g->frame)));
    }
    if (g->hurt_t > 0) {
        /* Fixed pattern, so renders are deterministic. Only the world shakes. */
        static const int8_t pattern[HURT_SHAKE_FRAMES][2] = { {1, -1}, {-1, 1}, {1, 1}, {-1, -1}, {1, 0}, {0, 1} };
        const int8_t* o = pattern[HURT_SHAKE_FRAMES - g->hurt_t];
        rl_offset(&g->rl, 0, o[0] * HURT_SHAKE_PX, o[1] * HURT_SHAKE_PX);
        g->rl.invert = g->hurt_t == HURT_SHAKE_FRAMES;
    }
    draw_particles(&g->rl, &g->cam, &g->particles);
    for (i = 0; i < MAX_IMPACTS; i++)
        if (g->impacts[i].t > 0) draw_impact(&g->rl, g->impacts[i].x, g->impacts[i].y, g->impacts[i].r);
    draw_weapon(&g->rl, &g->swing, &g->wrigs[g->weapon], IMG_WEAPON_HELD0 + (int)g->weapon);
    rl_hud_begin(&g->rl);   /* always: everything drawn from here on (HUD, panels) fits however busy the world was */
    if (g->mode == MODE_PLAY || g->mode == MODE_OFFER) hud_draw_play(&g->rl, g);
    if (g->mode == MODE_OFFER) {
        rl_rect(&g->rl, 0.0f, 0.0f, SCREEN_W, SCREEN_H, PAT_DIM, false);
        hud_draw_offer(&g->rl, g->weapon, g->offer);
    }
    if (g->mode == MODE_GAME_OVER) {   /* the world where the player fell, dimmed under the result */
        rl_rect(&g->rl, 0.0f, 0.0f, SCREEN_W, SCREEN_H, PAT_DIM, false);
        hud_draw_game_over(&g->rl, g);
    }
}
