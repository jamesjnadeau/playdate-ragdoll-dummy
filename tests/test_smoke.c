#include "test.h"

void suite_smoke(void) {
    CHECK(1 == 1);
    CHECK_NEAR(0.1f + 0.2f, 0.3f, 1e-6f);
}
