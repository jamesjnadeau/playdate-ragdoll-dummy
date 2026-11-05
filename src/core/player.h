#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include "input.h"
#include "hallway.h"
#include "dummy.h"

typedef struct {
    float z, x;
    int hearts, invuln, knock_frames;
    float knock_v;
    bool dead;
} Player;

typedef enum { PEV_NONE = 0, PEV_DOOR_SLAM = 1, PEV_DOOR_CLOSED = 2, PEV_HURT = 4 } PlayerEvent;

void player_init(Player* p);
/* Walks, blocks on dummies, gates doors and applies damage. Returns PlayerEvent bits. */
int player_update(Player* p, const GameInput* in, Hallway* h, DummyPool* dp, int incoming_damage);

#endif
