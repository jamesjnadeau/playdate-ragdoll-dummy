#ifndef DRAW_WORLD_H
#define DRAW_WORLD_H

#include "camera.h"
#include "render_list.h"
#include "hallway.h"
#include "dummy.h"
#include "rig.h"

/* Builds the world layer of a frame: hallway, doors, then far-to-near sorted dummies. */
void draw_world(RenderList* rl, const Camera* cam, const Hallway* hall,
                const DummyPool* dummies, const DummyRig rigs[DS_COUNT], int frame);

#endif
