#ifndef BENCH_H
#define BENCH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { BENCH_NONE, BENCH_WALK, BENCH_WAVE, BENCH_BURST_STORM, BENCH_SKINS } BenchScene;

typedef struct {
    BenchScene scene;
    float distance;
    uint32_t seed;
} BenchConfig;

/* Parses bench.json: {"scene":"walk|wave|burst_storm|skins","distance":N,"seed":N}.
 * Missing keys default to walk, 0, 1. Unknown scene or invalid JSON: false. */
bool bench_parse(const char* json, int len, BenchConfig* out);

#endif
