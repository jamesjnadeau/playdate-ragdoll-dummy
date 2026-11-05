#include <string.h>
#include "game.h"
#include "draw_world.h"
#include "tuning.h"

void game_init(Game* g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    rng_seed(&g->rng, seed);
    camera_set_aim(&g->cam, 1);
    g->cam.pitch_px = 0.0f;
}

void game_update(Game* g, const GameInput* in) {
    if (in->held & BTN_A) g->cam.player_z += PLAYER_SPEED * DT;
    if (in->held & BTN_B) g->cam.player_z -= PLAYER_SPEED * DT;
    if (g->cam.player_z < 0.0f) g->cam.player_z = 0.0f;
    if (in->pressed & BTN_UP) camera_set_aim(&g->cam, g->cam.aim + 1);
    if (in->pressed & BTN_DOWN) camera_set_aim(&g->cam, g->cam.aim - 1);
    camera_update(&g->cam);
}

void game_render(Game* g) {
    rl_clear(&g->rl);
    draw_world(&g->rl, &g->cam);
}
