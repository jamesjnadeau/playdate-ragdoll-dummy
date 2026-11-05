#include <math.h>

#include "pd_api.h"

static PlaydateAPI* g_pd = NULL;
static volatile float g_probe = 0.0f;
static volatile float g_probe_in = 0.5f;

static int update(void* userdata) {
    (void)userdata;
    g_pd->graphics->clear(kColorWhite);
    return 1;
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
        pd->system->setUpdateCallback(update, NULL);
    }
    return 0;
}
