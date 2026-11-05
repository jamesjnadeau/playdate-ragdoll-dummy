#include "weapons.h"
#include <stddef.h>

static const WeaponDef k_weapons[W_COUNT] = {
    {"Rubber Chicken", "weapon-chicken", 0.6f, 1.3f, 3.0f, 0.4f, 0.4f, 1.0f, 1, 1},
    {"Bat", "weapon-bat", 1.0f, 1.6f, 5.0f, 1.0f, 0.5f, 0.77f, 2, 3},
    {"Frying Pan", "weapon-pan", 1.4f, 1.4f, 6.0f, 1.4f, 0.5f, 0.67f, 3, 4},
    {"Sledgehammer", "weapon-sledge", 2.4f, 1.8f, 9.0f, 2.6f, 0.5f, 0.49f, 6, 6},
};

const WeaponDef* weapon_def(WeaponId id) {
    if ((int)id < 0 || (int)id >= (int)W_COUNT) return NULL;
    return &k_weapons[id];
}
