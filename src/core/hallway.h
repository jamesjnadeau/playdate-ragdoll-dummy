#ifndef HALLWAY_H
#define HALLWAY_H

#include "camera.h"
#include "render_list.h"
#include "rig.h"
#include "rng.h"

#define MAX_DOORS 12

typedef enum { DOOR_CLOSED_FULL, DOOR_OPEN, DOOR_CLOSED_DONE } DoorState;
typedef struct {
    int side;            /* -1 left, +1 right */
    float z;
    DoorState state;
    int remaining;       /* dummies still to come out */
    int spawn_timer;     /* frames until the next dummy while open */
    float leaf_angle;    /* 0 shut .. 1 fully open */
} Door;
typedef struct {
    Door doors[MAX_DOORS];
    int ndoors;
    float next_z;
    int doors_emptied;
    int recycled;        /* doors removed from the front of the array by the last update */
} Hallway;

void hallway_init(Hallway* h, Rng* rng);
int hallway_dummies_for_door(float z);        /* 1 + (int)(z/60), max 5 */
DummySize hallway_pick_size(Rng* rng, float z);
/* Opens doors within DOOR_ACTIVATE_DIST, generates doors ahead, recycles doors far behind. */
void hallway_update(Hallway* h, Rng* rng, float player_z);

/* Appends the hallway geometry (walls, ceiling, floor, far darkness, edges) to rl. */
void draw_hallway(RenderList* rl, const Camera* cam);

#endif
