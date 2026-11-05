#ifndef PD_ASSETS_H
#define PD_ASSETS_H

#include "pd_api.h"

/* Loads a bitmap. Tries path, then path + ".png"; logs and returns NULL on failure. */
LCDBitmap* pd_load_image(PlaydateAPI* pd, const char* path);

#endif
