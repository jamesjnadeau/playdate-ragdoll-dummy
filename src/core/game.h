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
} Game;

void game_init(Game* g, uint32_t seed);
/* Copies the platform-measured rigs (part sizes) into the game. */
void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]);
void game_update(Game* g, const GameInput* in);
/* Starts a benchmark scene. Other scenes behave like WALK until their task. */
void game_start_bench(Game* g, const BenchConfig* cfg);
/* Rebuilds g->rl for the current state. */
void game_render(Game* g);

#endif
