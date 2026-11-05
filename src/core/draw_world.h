#ifndef DRAW_WORLD_H
#define DRAW_WORLD_H

#include "camera.h"
#include "render_list.h"
#include "hallway.h"
#include "dummy.h"
#include "rig.h"
#include "swing.h"

/* Builds the world layer of a frame: hallway, doors, then far-to-near sorted dummies. */
void draw_world(RenderList* rl, const Camera* cam, const Hallway* hall,
                const DummyPool* dummies, const DummyRig rigs[DS_COUNT], int frame);

/* The weapon, screen-space: resting at the bottom right, or sweeping around a pivot below the
   screen with speed lines. No hands. image is the held-weapon ImageId. */
void draw_weapon(RenderList* rl, const Swing* s, const WeaponRig* wr, int image);
/* White 14-point star with a 2 px black outline, centred on (x, y) with outer radius r. */
void draw_impact(RenderList* rl, float x, float y, float r);

#endif
