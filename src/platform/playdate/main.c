#include <math.h>

#include "pd_api.h"
#include "game.h"
#include "pd_input.h"
#include "pd_render.h"
#include "pd_files.h"
#include "pd_assets.h"
#include "pd_sound.h"
#include "rig.h"
#include "bench.h"
#include "save.h"

static PlaydateAPI* g_pd = NULL;
static Game g_game;
static LCDBitmap* g_images[IMG_COUNT];
static DummyRig g_rigs[DS_COUNT];
static WeaponRig g_wrigs[WEAPON_COUNT];
static SaveData g_save;
static SaveRetry g_save_retry;
static volatile float g_probe = 0.0f;
static volatile float g_probe_in = 0.5f;

#define SAVE_PATH "save.txt"
#define SAVE_MAX_BYTES SAVE_BUF_BYTES

static void save_load(PlaydateAPI* pd) {
    int len = 0;
    char* txt = pd_read_file(pd, SAVE_PATH, &len, SAVE_MAX_BYTES);
    if (txt) save_read(txt, len, &g_save);
    else save_defaults(&g_save);
    if (txt) pd->system->realloc(txt, 0);
}

/* Writes the bests the game just earned, keeping the rest of the file (pack selections) as loaded.
   A failed write is retried a few times, spaced out, then given up on with one log line; the next new best tries again. */
static void save_flush(PlaydateAPI* pd) {
    char buf[SAVE_BUF_BYTES];
    int n;
    bool ok;
    if (!save_retry_due(&g_save_retry)) return;
    save_set_bests(&g_save, g_game.best_bags, (int)g_game.best_distance);
    n = save_write(&g_save, buf, (int)sizeof(buf));
    ok = n > 0 && pd_write_file(pd, SAVE_PATH, buf, n);
    switch (save_retry_report(&g_save_retry, ok)) {
    case SAVE_RETRY_OK: g_game.save_dirty = false; break;
    case SAVE_RETRY_AGAIN: break;
    case SAVE_RETRY_GAVE_UP:
        pd->system->logToConsole("save.txt write failed %d times: giving up until the next new best", SAVE_RETRY_MAX);
        g_game.save_dirty = false;
        break;
    }
}

static int update(void* userdata) {
    GameInput in;
    int i;
    (void)userdata;
    pd_input_read(g_pd, &in);
    game_update(&g_game, &in);
    for (i = 0; i < g_game.nsounds; i++) pd_sound_play(g_pd, (SoundId)g_game.sounds[i]);
    game_render(&g_game);
    if (g_game.save_dirty) save_flush(g_pd);
    pd_render_execute(g_pd, &g_game.rl);
    return 1;
}

/* bench.json (data dir only, never shipped) selects a benchmark scene and skips the title. */
#define BENCH_MAX_BYTES 65536

static void init_game(PlaydateAPI* pd) {
    BenchConfig cfg;
    int len = 0;
    char* json = pd_read_file(pd, "bench.json", &len, BENCH_MAX_BYTES);
    if (json && bench_parse(json, len, &cfg)) {
        pd->system->logToConsole("bench.json loaded: scene=%d distance=%d seed=%u",
                                 (int)cfg.scene, (int)cfg.distance, (unsigned)cfg.seed);
        game_start_bench(&g_game, &cfg);
    } else {
        if (json) pd->system->logToConsole("bench.json INVALID: falling back to a normal game");
        game_init(&g_game, (uint32_t)pd->system->getCurrentTimeMilliseconds());
        game_show_title(&g_game);
    }
    if (json) pd->system->realloc(json, 0);
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg) {
    (void)arg;
    if (event == kEventInit) {
        g_pd = pd;
        g_probe = sinf(g_probe_in); /* keeps the float toolchain path exercised by the double guard */
        pd->display->setRefreshRate(30);
        pd_assets_load_core(pd, g_images, g_rigs, g_wrigs);
        pd_render_set_images(g_images, IMG_COUNT);
        pd_render_prewarm(pd);
        pd_sound_init(pd, "packs/core/sounds");
        init_game(pd);
        save_load(pd);
        game_apply_bests(&g_game, g_save.best_bags, g_save.best_distance_m);
        game_set_rigs(&g_game, g_rigs);
        game_set_weapon_rigs(&g_game, g_wrigs);
        pd->system->setUpdateCallback(update, NULL);
    }
    return 0;
}
