#include "camera.h"
#include "tuning.h"

void camera_update(Camera* c) {
    c->pitch_px = CAM_PITCH_PX;
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
