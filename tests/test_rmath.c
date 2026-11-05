#include "test.h"
#include "rmath.h"

void suite_rmath(void) {
    CHECK_NEAR(rsin_deg(90.0f), 1.0f, 1e-3f);
    CHECK_NEAR(rcos_deg(180.0f), -1.0f, 1e-3f);
    CHECK_NEAR(rsin_deg(-30.0f), -0.5f, 2e-3f);
    CHECK_NEAR(rsin_deg(390.0f), 0.5f, 2e-3f);
    CHECK_NEAR(approachf(1.0f, 0.0f, 0.3f), 0.7f, 1e-6f);
    CHECK_NEAR(approachf(0.1f, 0.0f, 0.3f), 0.0f, 1e-6f);
    CHECK_NEAR(clampf(5.0f, 0.0f, 1.0f), 1.0f, 0);
}
