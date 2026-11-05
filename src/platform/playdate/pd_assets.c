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

static const char* const k_size_dir[DS_COUNT] = { "dummy-small", "dummy-medium", "dummy-large" };
static const char* const k_part_file[PART_COUNT] = {
    "base", "body", "head", "arm-upper", "arm-lower", "fist",
    "face-mean", "face-hurt", "face-dazed", "face-ko", "face-whack", "face-squish"
};
static const char* const k_weapon_dir[WEAPON_COUNT] = {
    "weapon-chicken", "weapon-bat", "weapon-pan", "weapon-sledge"
};

/* No snprintf: it drags newlib's stdio syscalls into the device link. */
static void join_core(char* out, size_t cap, const char* name) {
    static const char prefix[] = "packs/core/";
    size_t n = strlen(name);
    if (sizeof(prefix) - 1 + n + 1 > cap) { out[0] = 0; return; }
    memcpy(out, prefix, sizeof(prefix) - 1);
    memcpy(out + sizeof(prefix) - 1, name, n + 1);
}

static void numbered(char* out, const char* base, int n) { /* n is 1..9, out holds 32 bytes */
    size_t len = strlen(base);
    memcpy(out, base, len);
    out[len] = (char)('0' + n);
    out[len + 1] = 0;
}

static LCDBitmap* load_path(PlaydateAPI* pd, const char* dir, const char* file) {
    char path[128];
    size_t a = strlen(dir), b = strlen(file);
    if (a + b + 2 > sizeof(path)) return NULL;
    memcpy(path, dir, a);
    path[a] = '/';
    memcpy(path + a + 1, file, b + 1);
    return pd_load_image(pd, path);
}

static void bitmap_size(PlaydateAPI* pd, LCDBitmap* bmp, int16_t* w, int16_t* h) {
    int iw = 0, ih = 0;
    if (!bmp) { *w = 0; *h = 0; return; }
    pd->graphics->getBitmapData(bmp, &iw, &ih, NULL, NULL, NULL);
    *w = (int16_t)iw;
    *h = (int16_t)ih;
}

bool pd_assets_load_core(PlaydateAPI* pd, LCDBitmap* images[IMG_COUNT],
                         DummyRig rigs[DS_COUNT], WeaponRig wrigs[WEAPON_COUNT]) {
    char dir[96];
    char file[32];
    bool ok = true;
    int s, p, i;

    for (i = 0; i < IMG_COUNT; i++) images[i] = NULL;

    for (s = 0; s < DS_COUNT; s++) {
        rig_default((DummySize)s, &rigs[s]);
        join_core(dir, sizeof(dir), k_size_dir[s]);
        for (p = 0; p < PART_COUNT; p++) {
            images[img_dummy((DummySize)s, (DummyPart)p)] = load_path(pd, dir, k_part_file[p]);
        }
        /* optional faces fall back: whack -> hurt, squish -> dazed */
        if (!images[img_dummy((DummySize)s, PART_FACE_WHACK)])
            images[img_dummy((DummySize)s, PART_FACE_WHACK)] = images[img_dummy((DummySize)s, PART_FACE_HURT)];
        if (!images[img_dummy((DummySize)s, PART_FACE_SQUISH)])
            images[img_dummy((DummySize)s, PART_FACE_SQUISH)] = images[img_dummy((DummySize)s, PART_FACE_DAZED)];
        for (p = 0; p < PART_COUNT; p++) {
            LCDBitmap* bmp = images[img_dummy((DummySize)s, (DummyPart)p)];
            if (!bmp) ok = false;
            bitmap_size(pd, bmp, &rigs[s].part_w[p], &rigs[s].part_h[p]);
        }
    }

    for (i = 0; i < WEAPON_COUNT; i++) {
        join_core(dir, sizeof(dir), k_weapon_dir[i]);
        images[IMG_WEAPON_HELD0 + i] = load_path(pd, dir, "held");
        images[IMG_WEAPON_ICON0 + i] = load_path(pd, dir, "icon");
        if (!images[IMG_WEAPON_HELD0 + i] || !images[IMG_WEAPON_ICON0 + i]) ok = false;
        weapon_rig_default(i, &wrigs[i]);
        bitmap_size(pd, images[IMG_WEAPON_HELD0 + i], &wrigs[i].w, &wrigs[i].h);
    }

    for (i = 0; i < STUFF_PUFF_VARIANTS; i++) {
        numbered(file, "puff-", i + 1);
        images[IMG_PUFF0 + i] = load_path(pd, "packs/core/stuffing", file);
        if (!images[IMG_PUFF0 + i]) ok = false;
    }
    for (i = 0; i < STUFF_SCRAP_VARIANTS; i++) {
        numbered(file, "scrap-", i + 1);
        images[IMG_SCRAP0 + i] = load_path(pd, "packs/core/stuffing", file);
        if (!images[IMG_SCRAP0 + i]) ok = false;
    }

    images[IMG_UI_HEART_FULL] = load_path(pd, "ui", "heart-full");
    images[IMG_UI_HEART_EMPTY] = load_path(pd, "ui", "heart-empty");
    images[IMG_UI_BAG] = load_path(pd, "ui", "bag");
    if (!images[IMG_UI_HEART_FULL] || !images[IMG_UI_HEART_EMPTY] || !images[IMG_UI_BAG]) ok = false;

    if (!ok) pd->system->logToConsole("core pack: some images failed to load (see above)");
    return ok;
}
