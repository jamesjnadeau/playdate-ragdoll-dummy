#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "camera.h"
#include "render_list.h"
#include "rng.h"
#include "input.h"

typedef struct Game {
    Camera cam;
    RenderList rl;
    Rng rng;
} Game;

void game_init(Game* g, uint32_t seed);
void game_update(Game* g, const GameInput* in);
/* Rebuilds g->rl for the current state. */
void game_render(Game* g);

#endif
