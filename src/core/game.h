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

typedef struct Game {
    Camera cam;
    RenderList rl;
    Rng rng;
    Hallway hall;
    DummyPool dummies;
    int pending_damage;   /* damage dealt to the player, consumed by Task 9 */
} Game;

void game_init(Game* g, uint32_t seed);
void game_update(Game* g, const GameInput* in);
/* Starts a benchmark scene. Other scenes behave like WALK until their task. */
void game_start_bench(Game* g, const BenchConfig* cfg);
/* Rebuilds g->rl for the current state. */
void game_render(Game* g);

#endif
