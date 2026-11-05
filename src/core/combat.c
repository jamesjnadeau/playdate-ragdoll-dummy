#include "combat.h"
#include "tuning.h"

#define ARM_SIDE_OFFSET 0.6f

void combat_reach_area(const WeaponDef* w, int side, float* x0, float* x1, float* z0, float* z1) {
    float cx = (float)side * ARM_SIDE_OFFSET;
    *x0 = cx - REACH_PATCH_HALF_W;
    *x1 = cx + REACH_PATCH_HALF_W;
    *z0 = 0.0f;
    *z1 = w->reach;
}
