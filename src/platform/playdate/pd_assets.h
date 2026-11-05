#ifndef PD_ASSETS_H
#define PD_ASSETS_H

#include <stdbool.h>
#include "pd_api.h"
#include "rig.h"

/* Loads a bitmap. Tries path, then path + ".png"; logs and returns NULL on failure. */
LCDBitmap* pd_load_image(PlaydateAPI* pd, const char* path);

/* Loads the core pack (packs/core/...) and UI images into images[IMG_COUNT], filling the
   dummy rigs (anchors scaled by size, part_w/h from the bitmaps) and weapon rigs.
   Missing files are logged and left NULL; face-whack falls back to face-hurt and
   face-squish to face-dazed. Returns true when every required image loaded. */
bool pd_assets_load_core(PlaydateAPI* pd, LCDBitmap* images[IMG_COUNT],
                         DummyRig rigs[DS_COUNT], WeaponRig wrigs[WEAPON_COUNT]);

#endif
