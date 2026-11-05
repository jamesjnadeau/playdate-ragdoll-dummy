#include "test.h"
#include "rig.h"

void suite_rig(void) {
    DummyRig m, l, s;
    rig_default(DS_MEDIUM, &m);
    rig_default(DS_LARGE, &l);
    rig_default(DS_SMALL, &s);
    CHECK_NEAR(m.height_px, 120.0f, 1e-4f);
    CHECK_NEAR(l.shoulder_l.x, m.shoulder_l.x * 1.3f, 1e-4f);
    CHECK(s.neck.y < 0.0f);
    CHECK(s.neck.y > -s.height_px);
    CHECK_NEAR(s.height_px, 84.0f, 1e-4f);
    CHECK_NEAR(m.neck.y, -78.0f, 1e-4f);
    CHECK_NEAR(m.upper_arm_px, 20.0f, 1e-4f);
    CHECK_EQ_INT(img_dummy(DS_SMALL, PART_BASE), 0);
    CHECK_EQ_INT(img_dummy(DS_MEDIUM, PART_BODY), PART_COUNT + 1);
    CHECK(IMG_UI_BAG < IMG_COUNT);
}
