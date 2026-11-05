#ifndef RIG_H
#define RIG_H

#include <stdint.h>
#include "rmath.h"

typedef enum { DS_SMALL, DS_MEDIUM, DS_LARGE, DS_COUNT } DummySize;
typedef enum { PART_BASE, PART_BODY, PART_HEAD, PART_ARM_UPPER, PART_ARM_LOWER, PART_FIST,
               PART_FACE_MEAN, PART_FACE_HURT, PART_FACE_DAZED, PART_FACE_KO,
               PART_FACE_WHACK, PART_FACE_SQUISH, PART_COUNT } DummyPart;
#define WEAPON_COUNT 4
#define STUFF_PUFF_VARIANTS 4
#define STUFF_SCRAP_VARIANTS 4
enum {
  IMG_DUMMY0 = 0,                                     /* + size*PART_COUNT + part */
  IMG_WEAPON_HELD0 = IMG_DUMMY0 + DS_COUNT*PART_COUNT, /* + weapon */
  IMG_WEAPON_ICON0 = IMG_WEAPON_HELD0 + WEAPON_COUNT,
  IMG_PUFF0 = IMG_WEAPON_ICON0 + WEAPON_COUNT,
  IMG_SCRAP0 = IMG_PUFF0 + STUFF_PUFF_VARIANTS,
  IMG_UI_HEART_FULL = IMG_SCRAP0 + STUFF_SCRAP_VARIANTS, IMG_UI_HEART_EMPTY, IMG_UI_BAG,
  IMG_COUNT
};
static inline int img_dummy(DummySize s, DummyPart p) { return IMG_DUMMY0 + (int)s*PART_COUNT + (int)p; }

typedef struct {
  float height_px;
  Vec2 neck, shoulder_l, shoulder_r, face_center, body_center;
  float upper_arm_px, lower_arm_px;
  int16_t part_w[PART_COUNT], part_h[PART_COUNT];  /* filled by platform from loaded bitmaps */
} DummyRig;
typedef struct { Vec2 grip, tip; int16_t w, h; } WeaponRig;

void rig_default(DummySize s, DummyRig* out);  /* core pack anchors scaled by size */
/* Core pack grip/tip in image pixels (origin top-left), as measured by make_core_pack.py.
   w and h are left 0 for the platform to fill from the bitmap. */
void weapon_rig_default(int weapon, WeaponRig* out);

#endif
