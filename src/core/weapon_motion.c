#include "weapon_motion.h"
#include <math.h>
#include "tuning.h"

float weapon_wrap_deg(float a) {
    a = fmodf(a, 360.0f);
    if (a > 180.0f) a -= 360.0f;
    else if (a <= -180.0f) a += 360.0f;
    return a;
}

float weapon_pivot_x(const Weapon* wp) { return WEAPON_PIVOT_X + wp->shift; }

void weapon_detach(Weapon* wp, float angle, float vel) {
    wp->angle = angle;
    wp->vel = vel;
    wp->attached = false;
    wp->prev_diff = weapon_wrap_deg(wp->target - angle);
}

void weapon_update(Weapon* wp, const GameInput* in, const WeaponDef* w) {
    float goal = 0.0f;
    if ((in->held & BTN_LEFT) && !(in->held & BTN_RIGHT)) goal = -WEAPON_SHIFT_PX;
    else if ((in->held & BTN_RIGHT) && !(in->held & BTN_LEFT)) goal = WEAPON_SHIFT_PX;
    float ds = goal - wp->shift;
    if (ds > WEAPON_SHIFT_PX_PER_FRAME) ds = WEAPON_SHIFT_PX_PER_FRAME;
    if (ds < -WEAPON_SHIFT_PX_PER_FRAME) ds = -WEAPON_SHIFT_PX_PER_FRAME;
    wp->shift += ds;

    wp->prev_angle = wp->angle;
    wp->swept = false;

    if (in->crank_docked) {
        wp->live = wp->attached = false;
        wp->angle = wp->prev_angle = wp->target = wp->vel = 0.0f;
        return;
    }
    if (!wp->live) {
        wp->live = wp->attached = true;
        wp->target = wp->angle = wp->prev_angle = in->crank_angle - CRANK_ZERO_DEG;
        wp->vel = 0.0f;
        return;
    }

    wp->target += in->crank_delta;

    if (wp->attached) {
        float err = wp->target - wp->angle;
        wp->vel += err * w->spring / w->mass - wp->vel * w->damp;
        if (wp->vel > WEAPON_MAX_VEL) wp->vel = WEAPON_MAX_VEL;
        if (wp->vel < -WEAPON_MAX_VEL) wp->vel = -WEAPON_MAX_VEL;
        wp->angle += wp->vel;
        wp->swept = true;
    } else {
        wp->angle += wp->vel;
        wp->vel *= WEAPON_BOUNCE_DAMP;
        if (fabsf(wp->vel) < WEAPON_STOP_VEL) wp->vel = 0.0f;
        float diff = weapon_wrap_deg(wp->target - wp->angle);
        bool crossed = ((diff > 0.0f && wp->prev_diff < 0.0f) || (diff < 0.0f && wp->prev_diff > 0.0f)) &&
                       fabsf(diff) < 90.0f && fabsf(wp->prev_diff) < 90.0f;
        if (fabsf(diff) < WEAPON_RECONNECT_DEG || crossed) {
            wp->target = wp->angle + diff;
            wp->angle = wp->target;
            wp->vel = 0.0f;
            wp->attached = true;
        } else {
            wp->prev_diff = diff;
        }
    }

    if (fabsf(wp->angle) > WEAPON_REBASE_DEG) {
        float turns = 360.0f * (float)(int)(wp->angle / 360.0f);
        wp->angle -= turns;
        wp->prev_angle -= turns;
        wp->target -= turns;
    }
}
