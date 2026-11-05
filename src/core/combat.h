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
ReactionType combat_reaction(SwingSide side, SwingVert vert);

#endif
