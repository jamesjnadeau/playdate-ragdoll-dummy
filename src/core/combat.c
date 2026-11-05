#include <math.h>
#include "combat.h"
#include "tuning.h"

#define ARM_SIDE_OFFSET 0.6f
#define LAUNCH_SIDE 0.5f
#define LAUNCH_UP_UPWARD 0.8f
#define LAUNCH_UP_DOWNWARD 0.3f
#define LAUNCH_FORWARD 0.9f

ReactionType combat_reaction(SwingSide side, SwingVert vert) {
    if (side == SIDE_LEFT) return vert == VERT_UP ? RX_LEFT_UP : RX_LEFT_DOWN;
    if (side == SIDE_RIGHT) return vert == VERT_UP ? RX_RIGHT_UP : RX_RIGHT_DOWN;
    return vert == VERT_UP ? RX_UPWARD : RX_DOWNWARD;
}

/* Aim level that hits the head: small 0, medium 1, large 2. */
static int head_aim(DummySize s) { return (int)s; }

static Vec3 launch_vector(const Swing* s, const WeaponDef* w) {
    Vec3 v;
    float len, k;
    v.x = -(float)s->side * LAUNCH_SIDE;
    v.y = s->vert == VERT_UP ? LAUNCH_UP_UPWARD : LAUNCH_UP_DOWNWARD;
    v.z = LAUNCH_FORWARD;
    len = v3_len(v);
    k = w->launch * s->factor / len;
    return v3_scale(v, k);
}

void combat_reach_area(const WeaponDef* w, int side, float* x0, float* x1, float* z0, float* z1) {
    float cx = (float)side * ARM_SIDE_OFFSET;
    *x0 = cx - w->arc;
    *x1 = cx + w->arc;
    *z0 = 0.0f;
    *z1 = w->reach;
}

int combat_resolve(const Swing* s, const WeaponDef* w, int aim, float player_z,
                   DummyPool* pool, HitResult* out, int max_out) {
    int i, n = 0;
    float ax0, ax1, az0, az1;
    combat_reach_area(w, (int)s->side, &ax0, &ax1, &az0, &az1);
    for (i = 0; i < MAX_DUMMIES && n < max_out; i++) {
        Dummy* d = &pool->d[i];
        HitResult* h;
        float dz;
        if (!d->alive || d->state == DST_EMERGING || d->state == DST_RAGDOLL || d->state == DST_DEAD) continue;
        dz = d->z - player_z;
        if (!(dz > az0 && dz <= az1)) continue;
        if (d->x < ax0 || d->x > ax1) continue;

        h = &out[n++];
        h->dummy = i;
        h->head = aim == head_aim(d->size);
        h->damage = w->power * s->factor * (h->head ? HEAD_MULT : 1.0f);
        h->rx = combat_reaction(s->side, s->vert);
        h->launch.x = 0.0f; h->launch.y = 0.0f; h->launch.z = 0.0f;

        d->hp -= h->damage;
        d->reaction = (int)h->rx;
        d->reaction_t = REACTION_FRAMES;
        d->z += DUMMY_HIT_PUSHBACK * s->factor;
        h->knockout = d->hp <= 0.0f;
        if (h->knockout) {
            h->launch = launch_vector(s, w);
            d->launch = h->launch;
            d->state = DST_RAGDOLL;
            d->timer = 0;
        } else {
            d->state = DST_STAGGER;
            d->timer = DUMMY_STAGGER_FRAMES;
        }
    }
    return n;
}
