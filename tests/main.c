#include "test.h"

int g_failures = 0;

void suite_smoke(void);
void suite_rmath(void);
void suite_rng(void);
void suite_json(void);
void suite_camera(void);
void suite_render_list(void);
void suite_draw_world(void);
void suite_bench(void);
void suite_rig(void);

int main(void) {
    RUN(smoke);
    RUN(rmath);
    RUN(rng);
    RUN(json);
    RUN(camera);
    RUN(render_list);
    RUN(draw_world);
    RUN(bench);
    RUN(rig);
    if (g_failures > 0) {
        printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
