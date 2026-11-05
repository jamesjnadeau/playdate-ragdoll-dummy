#include <math.h>

#include "pd_api.h"
#include "game.h"
#include "pd_input.h"
#include "pd_render.h"
#include "pd_files.h"
#include "bench.h"

static PlaydateAPI* g_pd = NULL;
static Game g_game;
static volatile float g_probe = 0.0f;
static volatile float g_probe_in = 0.5f;

static int update(void* userdata) {
    GameInput in;
    (void)userdata;
    pd_input_read(g_pd, &in);
    game_update(&g_game, &in);
    game_render(&g_game);
    pd_render_execute(g_pd, &g_game.rl);
    return 1;
}

/* bench.json (data dir only, never shipped) selects a benchmark scene and skips the title. */
static void init_game(PlaydateAPI* pd) {
    BenchConfig cfg;
    int len = 0;
    char* json = pd_read_file(pd, "bench.json", &len);
    if (json && bench_parse(json, len, &cfg)) {
        game_start_bench(&g_game, &cfg);
    } else {
        game_init(&g_game, (uint32_t)pd->system->getCurrentTimeMilliseconds());
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
        init_game(pd);
        pd->system->setUpdateCallback(update, NULL);
    }
    return 0;
}
