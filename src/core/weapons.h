#ifndef WEAPONS_H
#define WEAPONS_H

typedef enum { W_CHICKEN, W_BAT, W_PAN, W_SLEDGE, W_COUNT } WeaponId;

typedef struct {
    const char* name;
    const char* folder;
    float power, reach, launch;
    float mass, spring, damp;
    int weight_bars, power_bars;
} WeaponDef;

/* Returns NULL for an out-of-range id. */
const WeaponDef* weapon_def(WeaponId id);

#endif
