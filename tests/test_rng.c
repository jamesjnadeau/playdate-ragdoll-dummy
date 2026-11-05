#include "test.h"
#include "rng.h"

void suite_rng(void) {
    Rng a, b;
    int i, same = 1;
    int seen[6] = {0, 0, 0, 0, 0, 0};
    int in_range = 1;
    int f_ok = 1;
    int zero_ok = 0;
    uint32_t first = 0;

    /* Same seed gives an identical first 100 values. */
    rng_seed(&a, 42);
    rng_seed(&b, 42);
    for (i = 0; i < 100; i++) {
        if (rng_u32(&a) != rng_u32(&b)) same = 0;
    }
    CHECK(same);

    /* rng_range stays inside [3,5] and hits all three values. */
    rng_seed(&a, 7);
    for (i = 0; i < 1000; i++) {
        int v = rng_range(&a, 3, 5);
        if (v < 3 || v > 5) in_range = 0;
        else seen[v] = 1;
    }
    CHECK(in_range);
    CHECK(seen[3] && seen[4] && seen[5]);

    /* rng_f01 stays in [0,1). */
    rng_seed(&a, 99);
    for (i = 0; i < 1000; i++) {
        float f = rng_f01(&a);
        if (!(f >= 0.0f && f < 1.0f)) f_ok = 0;
    }
    CHECK(f_ok);

    /* Seed 0 is not stuck at zero, and maps to 0x9E3779B9. */
    rng_seed(&a, 0);
    rng_seed(&b, 0x9E3779B9u);
    first = rng_u32(&a);
    CHECK_EQ_INT(first, rng_u32(&b));
    for (i = 0; i < 10; i++) {
        if (rng_u32(&a) != 0) zero_ok = 1;
    }
    CHECK(zero_ok);
}
