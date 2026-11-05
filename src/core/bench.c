#include <string.h>
#include "bench.h"
#include "json.h"

#define BENCH_MAX_TOKS 32

bool bench_parse(const char* json, int len, BenchConfig* out) {
    JsonTok t[BENCH_MAX_TOKS];
    char name[24];
    int n, i, iv;
    float fv;

    out->scene = BENCH_WALK;
    out->distance = 0.0f;
    out->seed = 1;
    if (!json || len <= 0) return false;
    n = json_parse(json, len, t, BENCH_MAX_TOKS);
    if (n < 1 || t[0].type != JT_OBJ) return false;

    i = json_obj_get(json, t, n, 0, "scene");
    if (i >= 0) {
        if (!json_str(json, t, i, name, (int)sizeof(name))) return false;
        if (strcmp(name, "walk") == 0) out->scene = BENCH_WALK;
        else if (strcmp(name, "wave") == 0) out->scene = BENCH_WAVE;
        else if (strcmp(name, "burst_storm") == 0) out->scene = BENCH_BURST_STORM;
        else if (strcmp(name, "skins") == 0) out->scene = BENCH_SKINS;
        else if (strcmp(name, "ragdoll") == 0) out->scene = BENCH_RAGDOLL;
        else return false;
    }
    i = json_obj_get(json, t, n, 0, "distance");
    if (i >= 0 && json_num_f(json, t, i, &fv)) out->distance = fv;
    i = json_obj_get(json, t, n, 0, "seed");
    if (i >= 0 && json_num_i(json, t, i, &iv) && iv >= 0) out->seed = (uint32_t)iv;
    return true;
}
