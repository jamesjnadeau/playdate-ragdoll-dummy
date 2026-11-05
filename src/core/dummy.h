#ifndef DUMMY_H
#define DUMMY_H

#include <stdbool.h>
#include "rig.h"

#define MAX_DUMMIES 16

typedef enum { DST_EMERGING, DST_WALK, DST_WINDUP, DST_PUNCH, DST_STAGGER, DST_RAGDOLL, DST_DEAD } DummyState;
typedef struct {
    bool alive; DummySize size; DummyState state; int timer;
    float x, z, hp; int door;          /* door index it came from, -1 none */
    int reaction; int reaction_t;      /* ReactionType (Task 11), frames left */
    int ragdoll;                       /* ragdoll index once knocked out, -1 */
    Vec3 launch;                       /* knockout launch vector, consumed by the ragdoll spawn (Task 13) */
} Dummy;
typedef struct { Dummy d[MAX_DUMMIES]; } DummyPool;

/* Returns the index, or -1 if the pool is full. */
int dummy_spawn(DummyPool* p, DummySize size, float x, float z, int door);
float dummy_height(DummySize s);
float dummy_radius(DummySize s);
float dummy_speed(DummySize s);
float dummy_max_hp(DummySize s);
int dummy_windup_frames(DummySize s);
int dummy_damage(DummySize s);
/* AI, separation, windup and punch. Sets *out_damage_to_player to the damage dealt this frame. */
void dummy_update_all(DummyPool* p, float player_z, float player_x, int* out_damage_to_player);

#endif
