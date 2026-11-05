#ifndef HUD_H
#define HUD_H

#include "render_list.h"
#include "weapons.h"

struct Game;
/* The in-play HUD (mockup A): hearts, distance, bags, weapon plate, swing indicator. Drawn after rl_hud_begin. */
void hud_draw_play(RenderList* rl, const struct Game* g);

/* The weapon offer panel: the current weapon on the left, the offered one on the right, each with
   icon, name, speed bars and power bars. Drawn over a dimmed world by the caller. */
void hud_draw_offer(RenderList* rl, WeaponId current, WeaponId offer);

/* The title screen text and plates (the hallway and the idle dummy are drawn by the caller). */
void hud_draw_title(RenderList* rl, const struct Game* g);
/* The game-over panel: bags, distance, bests and NEW BEST!. */
void hud_draw_game_over(RenderList* rl, const struct Game* g);

#endif
