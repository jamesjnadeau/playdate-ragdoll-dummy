#include "weapons.h"
#include <stddef.h>

/* power is per unit of momentum, so damage already carries mass: a body hit at the tip at 20 deg/frame
   does chicken 0.6, bat 1.0, pan 1.4, sledge 2.4 (the old canned swing's factor-1.0 damage). */
static const WeaponDef k_weapons[W_COUNT] = {
    {"Rubber Chicken", "weapon-chicken", 1.5f, 1.3f, 3.0f, 0.4f, 0.16f, 1.0f, 1, 1, true},
    {"Bat", "weapon-bat", 1.0f, 1.6f, 5.0f, 1.0f, 0.5f, 0.77f, 2, 3, false},
    {"Frying Pan", "weapon-pan", 1.01f, 1.4f, 6.0f, 1.4f, 0.5f, 0.56f, 3, 4, false},
    {"Sledgehammer", "weapon-sledge", 0.92f, 1.8f, 9.0f, 2.6f, 0.5f, 0.30f, 6, 6, false},
};

const WeaponDef* weapon_def(WeaponId id) {
    if ((int)id < 0 || (int)id >= (int)W_COUNT) return NULL;
    return &k_weapons[id];
}
