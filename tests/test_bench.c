#include "test.h"
#include "bench.h"
#include "game.h"
#include <string.h>

static bool parse(const char* s, BenchConfig* c) {
    return bench_parse(s, (int)strlen(s), c);
}

void suite_bench(void) {
    BenchConfig c;
    Game g;

    CHECK(parse("{\"scene\":\"wave\",\"distance\":400,\"seed\":7}", &c));
    CHECK_EQ_INT(c.scene, BENCH_WAVE);
    CHECK_NEAR(c.distance, 400.0f, 0.001f);
    CHECK_EQ_INT(c.seed, 7);

    CHECK(parse("{}", &c));
    CHECK_EQ_INT(c.scene, BENCH_WALK);
    CHECK_NEAR(c.distance, 0.0f, 0.001f);
    CHECK_EQ_INT(c.seed, 1);

    CHECK(parse("{\"scene\":\"burst_storm\"}", &c));
    CHECK_EQ_INT(c.scene, BENCH_BURST_STORM);
    CHECK(parse("{\"scene\":\"burst-storm\"}", &c));
    CHECK_EQ_INT(c.scene, BENCH_BURST_STORM);
    CHECK(parse("{\"scene\":\"skins\"}", &c));
    CHECK_EQ_INT(c.scene, BENCH_SKINS);
    CHECK(parse("{\"scene\":\"ragdoll\"}", &c));
    CHECK_EQ_INT(c.scene, BENCH_RAGDOLL);

    CHECK(!parse("{\"scene\":\"nope\"}", &c));
    CHECK(!parse("{not json", &c));
    CHECK(!parse("", &c));
    CHECK(!parse("[1,2]", &c));

    memset(&g, 0, sizeof(g));
    c.scene = BENCH_WALK;
    c.distance = 12.5f;
    c.seed = 3;
    game_start_bench(&g, &c);
    CHECK_NEAR(g.cam.player_z, 12.5f, 0.001f);
}
