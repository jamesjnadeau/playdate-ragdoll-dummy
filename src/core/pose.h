#ifndef POSE_H
#define POSE_H

#include "dummy.h"
#include "rig.h"
#include "render_list.h"
#include "camera.h"
#include "hallway.h"

typedef struct {
    float lean_deg, sx, sy, lift_m;                  /* whole-body */
    float head_dx, head_dy, head_rot_deg, head_sy;   /* head offset in rig px */
    float arm_l_deg, arm_r_deg, elbow_l_deg, elbow_r_deg; /* 0 = hanging down, + = outward */
    DummyPart face;
} DummyPose;

/* Idle/walk/windup/punch/stagger poses. frame is the global frame counter. */
void pose_for_state(const Dummy* d, int frame, DummyPose* out);
void draw_dummy(RenderList* rl, const Camera* cam, const Dummy* d, const DummyRig* rig, const DummyPose* pose);
/* World position of the open leaf's free edge (x, z). */
void door_leaf_free_edge(const Door* door, float* x, float* z);
void draw_door(RenderList* rl, const Camera* cam, const Door* door);

#endif
