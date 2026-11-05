#ifndef DRAW_WORLD_H
#define DRAW_WORLD_H

#include "camera.h"
#include "render_list.h"
#include "hallway.h"
#include "dummy.h"
#include "rig.h"
#include "swing.h"
#include "ragdoll.h"

/* Builds the world layer of a frame: hallway, the weapon's floor range patch (reach_weapon, NULL for none; reach_side
   -1/0/+1 from the D-pad), doors, then far-to-near sorted dummies and ragdolls (ragdolls may be NULL). */
void draw_world(RenderList* rl, const Camera* cam, const Hallway* hall,
                const DummyPool* dummies, const DummyRig rigs[DS_COUNT],
                const RagdollPool* ragdolls, int frame,
                const WeaponDef* reach_weapon, int reach_side);

/* The weapon, screen-space: resting at the bottom right, or sweeping around a pivot below the
   screen with speed lines. No hands. image is the held-weapon ImageId. */
void draw_weapon(RenderList* rl, const Swing* s, const WeaponRig* wr, int image);
/* A weapon icon lying on the floor at z, centred in the hallway, sized by distance. It does not bob by
   itself: the caller shifts the commands it appended (rl_offset) to bob it. Nothing when z is behind the near plane. */
void draw_pickup(RenderList* rl, const Camera* cam, float z, int icon_image);
/* White 14-point star with a 2 px black outline, centred on (x, y) with outer radius r. */
void draw_impact(RenderList* rl, float x, float y, float r);

#endif
