#include "rng.h"

#define RNG_DEFAULT_SEED 0x9E3779B9u

void rng_seed(Rng *r, uint32_t seed) {
    r->s = seed ? seed : RNG_DEFAULT_SEED;
}

uint32_t rng_u32(Rng *r) {
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

int rng_range(Rng *r, int lo, int hi_inclusive) {
    uint32_t span;
    if (hi_inclusive <= lo) return lo;
    span = (uint32_t)(hi_inclusive - lo) + 1u;
    return lo + (int)(rng_u32(r) % span);
}

float rng_f01(Rng *r) {
    return (float)(rng_u32(r) >> 8) * (1.0f / 16777216.0f);
}
