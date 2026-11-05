#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include "rmath.h"

typedef struct {
    float player_z;
    float pitch_px;
    int aim;            /* 0 low, 1 mid, 2 high */
} Camera;

/* Sets the aim level, clamped to 0..2. */
void camera_set_aim(Camera* c, int aim);
/* Eases pitch_px toward the target for the current aim by AIM_EASE_PX_PER_FRAME. */
void camera_update(Camera* c);
/* Projects world point w to screen. Returns false when w is closer than NEAR_Z. */
bool camera_project(const Camera* c, Vec3 w, float* sx, float* sy, float* px_per_m);

#endif
