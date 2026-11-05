#include "test.h"
#include "camera.h"
#include "tuning.h"

void suite_camera(void) {
    Camera c = {0.0f, 0.0f, 1};
    Vec3 w;
    float sx = 0.0f, sy = 0.0f, ppm = 0.0f;

    /* Centre point: dz = 1.5, so px_per_m = 100 and sy lands on the horizon. */
    w.x = 0.0f; w.y = CAM_Y; w.z = 1.5f;
    CHECK(camera_project(&c, w, &sx, &sy, &ppm));
    CHECK_NEAR(sx, 200.0f, 1e-3f);
    CHECK_NEAR(sy, HORIZON_Y, 1e-3f);
    CHECK_NEAR(ppm, 100.0f, 1e-3f);

    /* Floor point at the right wall, dz = 3: sx = 280, sy = HORIZON_Y + 67.5 (CAM_Y * FOCAL / 3). */
    w.x = HALL_HALF_W; w.y = 0.0f; w.z = 3.0f;
    CHECK(camera_project(&c, w, &sx, &sy, &ppm));
    CHECK_NEAR(sx, 280.0f, 1e-3f);
    CHECK_NEAR(sy, HORIZON_Y + 67.5f, 1e-3f);

    /* Closer than NEAR_Z relative to the player is rejected. */
    c.player_z = 0.0f;
    w.x = 0.0f; w.y = CAM_Y; w.z = c.player_z + 0.1f;
    CHECK(!camera_project(&c, w, &sx, &sy, &ppm));

    /* The hallway clips quads to exactly player_z + NEAR_Z: that must project for any player_z. */
    for (int i = 0; i < 400; i++) {
        c.player_z = 0.0137f * (float)i;
        w.z = c.player_z + NEAR_Z;
        CHECK(camera_project(&c, w, &sx, &sy, &ppm));
    }

    /* Aim eases toward its target by AIM_EASE_PX_PER_FRAME per update. */
    camera_set_aim(&c, 0);
    camera_update(&c);
    camera_update(&c);
    camera_update(&c);
    CHECK_NEAR(c.pitch_px, -24.0f, 1e-4f);
    for (int i = 0; i < 7; i++) camera_update(&c);
    CHECK_NEAR(c.pitch_px, -40.0f, 1e-4f);

    /* Out-of-range aim is clamped to 0..2. */
    camera_set_aim(&c, 7);
    CHECK_EQ_INT(c.aim, 2);
    camera_set_aim(&c, -3);
    CHECK_EQ_INT(c.aim, 0);
}
