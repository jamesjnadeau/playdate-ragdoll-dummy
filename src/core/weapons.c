#include "weapons.h"
#include <stddef.h>

static const WeaponDef k_weapons[W_COUNT] = {
    {"Rubber Chicken", "weapon-chicken", 2, 0.6f, 1.3f, 1.0f, 3.0f, 6, 1},
    {"Bat", "weapon-bat", 5, 1.0f, 1.6f, 1.2f, 5.0f, 4, 3},
    {"Frying Pan", "weapon-pan", 7, 1.4f, 1.4f, 1.0f, 6.0f, 3, 4},
    {"Sledgehammer", "weapon-sledge", 12, 2.4f, 1.8f, 1.4f, 9.0f, 1, 6},
};

const WeaponDef* weapon_def(WeaponId id) {
    if ((int)id < 0 || (int)id >= (int)W_COUNT) return NULL;
    return &k_weapons[id];
}
