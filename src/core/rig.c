#include <string.h>
#include "rig.h"

void rig_default(DummySize s, DummyRig* out) {
    static const float heights[DS_COUNT] = { 84.0f, 120.0f, 156.0f };
    float k;
    memset(out, 0, sizeof(*out));
    out->height_px = heights[(int)s];
    k = out->height_px / 120.0f;
    out->neck = (Vec2){ 0.0f, -78.0f * k };
    out->shoulder_l = (Vec2){ -20.0f * k, -64.0f * k };
    out->shoulder_r = (Vec2){ 20.0f * k, -64.0f * k };
    out->face_center = (Vec2){ 0.0f, -94.0f * k };
    out->body_center = (Vec2){ 0.0f, -45.0f * k };
    out->upper_arm_px = 20.0f * k;
    out->lower_arm_px = 18.0f * k;
}

void weapon_rig_default(int weapon, WeaponRig* out) {
    /* chicken, bat, pan, sledge; keep in sync with the anchors in Source/packs/core/pack.json */
    static const int16_t grip[WEAPON_COUNT][2] = { {30, 170}, {30, 171}, {30, 169}, {30, 173} };
    static const int16_t tip[WEAPON_COUNT][2] = { {30, 2}, {30, 2}, {30, 2}, {30, 4} };
    memset(out, 0, sizeof(*out));
    if (weapon < 0 || weapon >= WEAPON_COUNT) return;
    out->grip = (Vec2){ (float)grip[weapon][0], (float)grip[weapon][1] };
    out->tip = (Vec2){ (float)tip[weapon][0], (float)tip[weapon][1] };
}
