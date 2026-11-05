#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "camera.h"
#include "render_list.h"
#include "rng.h"
#include "input.h"
#include "bench.h"
#include "hallway.h"
#include "dummy.h"
#include "player.h"
#include "rig.h"
#include "swing.h"
#include "combat.h"
#include "ragdoll.h"
#include "particles.h"
#include "tuning.h"

typedef struct { float x, y, r; int t; } Impact;   /* screen star, t = frames left */

typedef struct Game {
    Camera cam;
    RenderList rl;
    Rng rng;
    Hallway hall;
    DummyPool dummies;
    Player player;
    DummyRig rigs[DS_COUNT];
    int pending_damage;   /* damage dealt to the player, consumed by the next player_update */
    int frame;
    Swing swing;
    WeaponId weapon;
    bool prev_docked;     /* crank dock state last frame, to swallow the undock jump */
    HitResult hits[16];   /* this frame's hits, for effects and sounds */
    int nhits;
    WeaponRig wrigs[WEAPON_COUNT];   /* grip/tip and image size of each held weapon */
    Impact impacts[MAX_IMPACTS];
    RagdollPool ragdolls;
    Vec3 bursts[6];       /* where ragdolls burst this frame */
    Vec3 burst_n[6];      /* surface normal of each burst (away from the wall, floor or ceiling) */
    int nbursts;
    ParticlePool particles;
    BenchScene bench;     /* BENCH_NONE for a normal game */
    int hurt_t;           /* frames of hurt shake left; the frame it starts on also inverts */
} Game;

void game_init(Game* g, uint32_t seed);
/* Copies the platform-measured rigs (part sizes) into the game. */
void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]);
/* Copies the platform-measured weapon rigs (image sizes) into the game. */
void game_set_weapon_rigs(Game* g, const WeaponRig wrigs[WEAPON_COUNT]);
void game_update(Game* g, const GameInput* in);
/* Starts a benchmark scene. Other scenes behave like WALK until their task. */
void game_start_bench(Game* g, const BenchConfig* cfg);
/* Rebuilds g->rl for the current state. */
void game_render(Game* g);

#endif
