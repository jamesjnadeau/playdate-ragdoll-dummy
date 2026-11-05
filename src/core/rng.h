#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* xorshift32 generator. State 0 is never used (seed 0 maps to 0x9E3779B9). */
typedef struct { uint32_t s; } Rng;

void rng_seed(Rng *r, uint32_t seed);
uint32_t rng_u32(Rng *r);
/* Uniform integer in [lo, hi_inclusive]. */
int rng_range(Rng *r, int lo, int hi_inclusive);
/* Uniform float in [0, 1). */
float rng_f01(Rng *r);

#endif
