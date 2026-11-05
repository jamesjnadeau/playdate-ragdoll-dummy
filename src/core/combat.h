#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include "weapons.h"
#include "dummy.h"
#include "rmath.h"

typedef enum { RX_NONE, RX_FROM_LEFT, RX_FROM_RIGHT, RX_UPWARD, RX_DOWNWARD,
               RX_LEFT_UP, RX_LEFT_DOWN, RX_RIGHT_UP, RX_RIGHT_DOWN } ReactionType;
typedef struct { int dummy; float damage; bool head; bool knockout; ReactionType rx; Vec3 launch;
                 float sx, sy, m; } HitResult;   /* sx, sy: screen contact point; m: contact momentum (contact.c) */

/* The floor patch drawn as a reach hint for `side` (-1 left, 0 centre, +1 right), relative to the player:
   lateral x0..x1 (not clamped to the walls) and forward z0..z1. Hits themselves come from contact_sweep. */
void combat_reach_area(const WeaponDef* w, int side, float* x0, float* x1, float* z0, float* z1);

#endif
