#ifndef HALLWAY_H
#define HALLWAY_H

#include "camera.h"
#include "render_list.h"

/* Appends the hallway geometry (walls, ceiling, floor, far darkness, edges) to rl. */
void draw_hallway(RenderList* rl, const Camera* cam);

#endif
