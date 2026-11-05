#ifndef WEAPON_MOTION_H
#define WEAPON_MOTION_H

#include <stdbool.h>
#include <stdint.h>
#include "input.h"
#include "weapons.h"

typedef struct {
    float angle, prev_angle;   /* degrees, unwrapped; prev_angle is the start of this frame's move */
    float vel;                 /* degrees per frame, signed */
    float target;              /* crank target angle, unwrapped */
    float shift;               /* pivot x offset from WEAPON_PIVOT_X, px */
    float prev_diff;           /* wrapped target - angle at the end of the last loose frame */
    bool live;                 /* false while docked / before the first undocked frame */
    bool attached;
    bool swept;                /* this frame's prev_angle -> angle move was a spring step and may hit */
    uint16_t inside;           /* bit i: the weapon overlaps dummy i (owned by contact.c) */
    uint16_t struck;           /* bit i: dummy i was hit this pass and counts as inside (owned by contact.c) */
    float struck_dir;          /* sign of the move that struck them (owned by contact.c) */
    float struck_travel;       /* degrees turned since the last strike (owned by contact.c) */
} Weapon;                      /* all-zero is a valid docked, centred weapon */

float weapon_wrap_deg(float a);                 /* to (-180, 180] */
float weapon_pivot_x(const Weapon* wp);         /* WEAPON_PIVOT_X + shift */
void weapon_update(Weapon* wp, const GameInput* in, const WeaponDef* w);
void weapon_detach(Weapon* wp, float angle, float vel); /* used by contact.c on a bounce */

#endif
