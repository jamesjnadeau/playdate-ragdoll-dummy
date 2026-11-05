#include "test.h"
#include "combat.h"
#include "tuning.h"

/* The floor patch drawn under the weapon: a fixed width around the held side, out to the weapon's reach. */
static void test_reach_area(void) {
    for (int wi = 0; wi < W_COUNT; wi++) {
        const WeaponDef* w = weapon_def((WeaponId)wi);
        for (int side = -1; side <= 1; side++) {
            float x0, x1, z0, z1;
            combat_reach_area(w, side, &x0, &x1, &z0, &z1);
            CHECK_NEAR(0.5f * (x0 + x1), (float)side * 0.6f, 0.0001f);
            CHECK_NEAR(x1 - x0, 2.0f * REACH_PATCH_HALF_W, 0.0001f);
            CHECK_NEAR(z0, 0.0f, 0.0001f);
            CHECK_NEAR(z1, w->reach, 0.0001f);
        }
    }
}

void suite_combat(void) {
    test_reach_area();
}
