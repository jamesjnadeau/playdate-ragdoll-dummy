#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include "swing.h"
#include "weapons.h"
#include "dummy.h"
#include "rmath.h"

typedef enum { RX_NONE, RX_FROM_LEFT, RX_FROM_RIGHT, RX_UPWARD, RX_DOWNWARD,
               RX_LEFT_UP, RX_LEFT_DOWN, RX_RIGHT_UP, RX_RIGHT_DOWN } ReactionType;
typedef struct { int dummy; float damage; bool head; bool knockout; ReactionType rx; Vec3 launch; } HitResult;

/* Hits every dummy in reach and arc (never EMERGING/RAGDOLL/DEAD). Returns the number of hits written to out. */
int combat_resolve(const Swing* s, const WeaponDef* w, int aim, float player_z,
                   DummyPool* pool, HitResult* out, int max_out);
/* The floor area a swing on `side` (-1 left, 0 centre, +1 right) can hit, relative to the player: lateral x0..x1
   (not clamped to the walls) and forward z0..z1 (z0 is exclusive, as in the hit test). combat_resolve uses it. */
void combat_reach_area(const WeaponDef* w, int side, float* x0, float* x1, float* z0, float* z1);
ReactionType combat_reaction(SwingSide side, SwingVert vert);

#endif
