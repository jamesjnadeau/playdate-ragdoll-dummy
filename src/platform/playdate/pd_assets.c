#include <string.h>
#include "pd_assets.h"

LCDBitmap* pd_load_image(PlaydateAPI* pd, const char* path) {
    char alt[128];
    const char* err = NULL;
    LCDBitmap* bmp = pd->graphics->loadBitmap(path, &err);
    size_t n = strlen(path);
    if (bmp) return bmp;
    if (n + 5 <= sizeof(alt)) {
        memcpy(alt, path, n);
        memcpy(alt + n, ".png", 5);
        err = NULL;
        bmp = pd->graphics->loadBitmap(alt, &err);
        if (bmp) return bmp;
    }
    pd->system->logToConsole("pd_load_image failed: %s (%s)", path, err ? err : "unknown");
    return NULL;
}
