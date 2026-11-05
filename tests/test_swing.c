#include "test.h"
#include "swing.h"
#include "tuning.h"
#include <string.h>

static GameInput mk(float d, int held, bool docked) {
    GameInput in;
    memset(&in, 0, sizeof in);
    in.crank_delta = d;
    in.held = (uint8_t)held;
    in.crank_docked = docked;
    return in;
}

void suite_swing(void) {
    int f;
    const WeaponDef* bat = weapon_def(W_BAT);
    const WeaponDef* chk = weapon_def(W_CHICKEN);

    /* weapons table */
    CHECK(weapon_def(W_SLEDGE) != NULL);
    CHECK_EQ_INT(weapon_def(W_PAN)->delay, 7);
    CHECK_EQ_INT(weapon_def(W_SLEDGE)->power_bars, 6);
    CHECK(strcmp(weapon_def(W_CHICKEN)->name, "Rubber Chicken") == 0);
    CHECK(strcmp(weapon_def(W_BAT)->folder, "weapon-bat") == 0);
    CHECK(weapon_def((WeaponId)-1) == NULL);
    CHECK(weapon_def((WeaponId)4) == NULL);
    CHECK(weapon_def((WeaponId)99) == NULL);

    /* basic timing */
    {
        Swing s;
        int impacts = 0, impact_frame = 0, started_frame = 0, ready_frame = 0;
        memset(&s, 0, sizeof s);
        for (f = 1; f <= 40; f++) {
            GameInput in = mk(f <= 4 ? 30.0f : 0.0f, 0, false);
            SwingEvent e = swing_update(&s, &in, bat);
            if (e == SWE_STARTED && !started_frame) started_frame = f;
            if (e == SWE_IMPACT) { impacts++; impact_frame = f; }
            if (f > 1 && s.phase == SWS_READY && !ready_frame) ready_frame = f;
            if (f == 1) {
                CHECK_EQ_INT(s.side, SIDE_CENTER);
                CHECK_EQ_INT(s.vert, VERT_DOWN);
            }
            if (f == 5) CHECK_NEAR(s.factor, 1.0f, 1e-3f);
        }
        CHECK_EQ_INT(started_frame, 1);
        CHECK_EQ_INT(impacts, 1);
        CHECK_EQ_INT(impact_frame, 1 + 4 + 5 + 3);
        CHECK_EQ_INT(ready_frame, 1 + 4 + 5 + 6 + 8);
    }

    /* side and up */
    {
        Swing s;
        GameInput in = mk(-20.0f, BTN_LEFT, false);
        memset(&s, 0, sizeof s);
        CHECK_EQ_INT(swing_update(&s, &in, bat), SWE_STARTED);
        in = mk(0.0f, BTN_LEFT, false);
        for (f = 0; f < 4; f++) swing_update(&s, &in, bat);
        CHECK_EQ_INT(s.side, SIDE_LEFT);
        CHECK_EQ_INT(s.vert, VERT_UP);
        CHECK_NEAR(s.factor, 0.6667f, 1e-3f);
    }

    /* docked */
    {
        Swing s;
        int started = 0;
        GameInput in = mk(50.0f, 0, true);
        memset(&s, 0, sizeof s);
        for (f = 0; f < 30; f++)
            if (swing_update(&s, &in, bat) == SWE_STARTED) started = 1;
        CHECK(!started);
        CHECK_EQ_INT(s.phase, SWS_READY);
    }

    /* docking mid-swing: swing finishes */
    {
        Swing s;
        int impacts = 0;
        GameInput in = mk(30.0f, 0, false);
        memset(&s, 0, sizeof s);
        swing_update(&s, &in, bat);
        in = mk(0.0f, 0, true);
        for (f = 0; f < 30; f++)
            if (swing_update(&s, &in, bat) == SWE_IMPACT) impacts++;
        CHECK_EQ_INT(impacts, 1);
        CHECK_EQ_INT(s.phase, SWS_READY);
    }

    /* huge jump clamped */
    {
        Swing s;
        GameInput in = mk(400.0f, 0, false);
        memset(&s, 0, sizeof s);
        swing_update(&s, &in, bat);
        in = mk(0.0f, 0, false);
        for (f = 0; f < 4; f++) swing_update(&s, &in, bat);
        CHECK_NEAR(s.factor, SWING_FACTOR_MAX, 1e-6f);
    }

    /* no double swings */
    {
        Swing s;
        int impacts = 0, frames[8] = {0};
        GameInput in = mk(30.0f, 0, false);
        memset(&s, 0, sizeof s);
        for (f = 1; f <= 60; f++) {
            if (swing_update(&s, &in, chk) == SWE_IMPACT) {
                if (impacts < 8) frames[impacts] = f;
                impacts++;
            }
        }
        CHECK_EQ_INT(impacts, 3);
        CHECK_EQ_INT(frames[0], 10);
        CHECK_EQ_INT(frames[1], 31);
        CHECK_EQ_INT(frames[2], 52);
    }

    /* reversal ignored in measure */
    {
        Swing s;
        float ds[4] = {30.0f, -40.0f, -40.0f, -40.0f};
        memset(&s, 0, sizeof s);
        for (f = 0; f < 4; f++) {
            GameInput in = mk(ds[f], 0, false);
            swing_update(&s, &in, bat);
        }
        {
            GameInput in = mk(0.0f, 0, false);
            swing_update(&s, &in, bat);
        }
        CHECK_NEAR(s.factor, 1.0f, 1e-3f);
        CHECK_EQ_INT(s.vert, VERT_DOWN);
    }
}
