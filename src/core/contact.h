#ifndef CONTACT_H
#define CONTACT_H

#include "weapon_motion.h"
#include "weapons.h"
#include "rig.h"
#include "camera.h"
#include "dummy.h"
#include "combat.h"

/* Grip and tip on screen for the weapon at angle_deg about the current pivot. */
void  weapon_segment(const Weapon* wp, const WeaponRig* wr, float angle_deg,
                     float* gx, float* gy, float* tx, float* ty);
bool  contact_heavy(const WeaponDef* w, float m);      /* a hit that plays the heavy sound */
float weapon_tip_radius(const WeaponRig* wr);            /* WEAPON_SWEEP_RADIUS + |tip - grip| */
float contact_momentum(const WeaponDef* w, float vel_deg, float r_px);  /* mass * |vel|*pi/180 * r */
float dummy_resist(DummySize s);
ReactionType contact_reaction(float angle_deg, float vel);
/* Sweeps wp from prev_angle to angle against every in-reach dummy, applies hits, and may detach wp (bounce).
   Always refreshes wp->inside. Returns the number of HitResults written (<= max_out). */
int contact_sweep(Weapon* wp, const WeaponDef* w, const WeaponRig* wr, const Camera* cam,
                  DummyPool* pool, const DummyRig rigs[DS_COUNT], int frame,
                  HitResult* out, int max_out);

#endif
