#include "camera.h"
#include "tuning.h"

static float aim_target_px(int aim) {
    if (aim <= 0) return AIM_PITCH_LOW_PX;
    if (aim >= 2) return AIM_PITCH_HIGH_PX;
    return 0.0f;
}

void camera_set_aim(Camera* c, int aim) {
    if (aim < 0) aim = 0;
    if (aim > 2) aim = 2;
    c->aim = aim;
}

void camera_update(Camera* c) {
    c->pitch_px = approachf(c->pitch_px, aim_target_px(c->aim), AIM_EASE_PX_PER_FRAME);
}

bool camera_project(const Camera* c, Vec3 w, float* sx, float* sy, float* px_per_m) {
    float dz = w.z - c->player_z;
    float k;
    /* Tolerance: (player_z + NEAR_Z) - player_z can round just below NEAR_Z, and the hallway clips
       its quads exactly to that plane. */
    if (dz < NEAR_Z - NEAR_Z_EPS) return false;
    k = FOCAL / dz;
    *sx = SCREEN_CX + w.x * k;
    *sy = HORIZON_Y + c->pitch_px - (w.y - CAM_Y) * k;
    *px_per_m = k;
    return true;
}
