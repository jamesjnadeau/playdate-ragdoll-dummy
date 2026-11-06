#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include "rmath.h"

typedef struct {
    float player_z;
    float pitch_px;     /* horizon offset; the view is fixed at CAM_PITCH_PX */
} Camera;

/* Holds the view at CAM_PITCH_PX, halfway between looking down and level. */
void camera_update(Camera* c);
/* Projects world point w to screen. Returns false when w is closer than NEAR_Z. */
bool camera_project(const Camera* c, Vec3 w, float* sx, float* sy, float* px_per_m);

#endif
