#ifndef DRAW_WORLD_H
#define DRAW_WORLD_H

#include "camera.h"
#include "render_list.h"
#include "hallway.h"

/* Builds the whole world layer of a frame (currently just the hallway). */
void draw_world(RenderList* rl, const Camera* cam);

#endif
