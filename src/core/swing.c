#include "swing.h"
#include "tuning.h"
#include "rmath.h"
#include <math.h>

#define SWING_DELTA_CLAMP 90.0f

SwingEvent swing_update(Swing* s, const GameInput* in, const WeaponDef* w) {
    float d = in->crank_docked ? 0.0f : clampf(in->crank_delta, -SWING_DELTA_CLAMP, SWING_DELTA_CLAMP);

    switch (s->phase) {
    case SWS_READY:
        if (fabsf(d) >= SWING_TRIGGER_DEG) {
            s->phase = SWS_MEASURE;
            s->t = 0;
            s->side = (in->held & BTN_LEFT) ? SIDE_LEFT : (in->held & BTN_RIGHT) ? SIDE_RIGHT : SIDE_CENTER;
            s->vert = d > 0.0f ? VERT_DOWN : VERT_UP;
            s->peak = fabsf(d);
            s->factor = 0.0f;
            s->sweep01 = 0.0f;
            return SWE_STARTED;
        }
        return SWE_NONE;
    case SWS_MEASURE: {
        float along = (float)s->vert * d; /* positive when in the starting direction */
        if (along > s->peak) s->peak = along;
        s->t++;
        if (s->t >= SWING_WINDOW) {
            s->factor = clampf(s->peak / SWING_SPEED_REF, SWING_FACTOR_MIN, SWING_FACTOR_MAX);
            s->phase = SWS_DELAY;
            s->t = 0;
        }
        return SWE_NONE;
    }
    case SWS_DELAY:
        s->t++;
        if (s->t >= w->delay) {
            s->phase = SWS_SWEEP;
            s->t = 0;
            s->sweep01 = 0.0f;
        }
        return SWE_NONE;
    case SWS_SWEEP: {
        float before, after;
        before = s->sweep01;
        s->t++;
        after = (float)s->t / (float)SWING_SWEEP_FRAMES;
        if (after > 1.0f) after = 1.0f;
        s->sweep01 = after;
        if (s->t >= SWING_SWEEP_FRAMES) {
            s->phase = SWS_RECOVER;
            s->t = 0;
        }
        return (before < 0.5f && after >= 0.5f) ? SWE_IMPACT : SWE_NONE;
    }
    case SWS_RECOVER:
        s->t++;
        if (s->t >= SWING_RECOVER_FRAMES) {
            s->phase = SWS_READY;
            s->t = 0;
        }
        return SWE_NONE;
    }
    return SWE_NONE;
}
