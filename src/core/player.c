#include "player.h"
#include "tuning.h"

#define BLOCK_LANE_MARGIN 0.6f

void player_init(Player* p) {
    p->z = 0.0f;
    p->x = 0.0f;
    p->hearts = PLAYER_HEARTS;
    p->invuln = 0;
    p->knock_frames = 0;
    p->knock_v = 0.0f;
    p->min_z = 0.0f;
    p->dead = false;
}

/* Smallest z the player may reach before touching a dummy ahead in the lane. */
static float block_limit(const Player* p, const DummyPool* dp) {
    float lim = 1e30f;
    int i;
    for (i = 0; i < MAX_DUMMIES; i++) {
        const Dummy* d = &dp->d[i];
        float r, l;
        if (!d->alive || d->state == DST_RAGDOLL || d->state == DST_DEAD) continue;
        r = dummy_radius(d->size);
        if (d->x - p->x >= r + BLOCK_LANE_MARGIN || p->x - d->x >= r + BLOCK_LANE_MARGIN) continue;
        l = d->z - r - PLAYER_RADIUS;
        if (l < lim) lim = l;
    }
    return lim;
}

float player_back_floor(const Player* p, const Hallway* h) {
    float f = p->min_z, nearest = 1e30f;
    int i;
    for (i = 0; i < h->ndoors; i++) {
        const Door* d = &h->doors[i];
        if (d->state == DOOR_CLOSED_DONE && d->z > f) f = d->z;
        if (d->state == DOOR_OPEN && d->z >= p->z - WALK_BACK_LIMIT && d->z < nearest) nearest = d->z;
    }
    if (nearest < 1e29f && nearest - WALK_BACK_LIMIT > f) f = nearest - WALK_BACK_LIMIT;
    return f;
}

static bool door_has_emerging(const DummyPool* dp, int door) {
    int i;
    for (i = 0; i < MAX_DUMMIES; i++)
        if (dp->d[i].alive && dp->d[i].door == door && dp->d[i].state == DST_EMERGING) return true;
    return false;
}

int player_update(Player* p, const GameInput* in, Hallway* h, DummyPool* dp, int incoming_damage) {
    int ev = PEV_NONE, i;
    bool knocked = false;
    float prev = p->z, floor_z;
    float hard;
    for (i = 0; i < h->ndoors; i++)
        if (h->doors[i].state == DOOR_CLOSED_DONE && h->doors[i].z > p->min_z) p->min_z = h->doors[i].z;
    hard = p->min_z;
    floor_z = player_back_floor(p, h);

    if (p->invuln > 0) p->invuln--;
    if (incoming_damage > 0 && p->invuln == 0 && !p->dead) {
        p->hearts -= incoming_damage;
        if (p->hearts <= 0) { p->hearts = 0; p->dead = true; }
        p->invuln = PLAYER_INVULN_FRAMES;
        ev |= PEV_HURT;
    }
    if (p->dead) return ev;

    if (p->knock_frames > 0) {
        knocked = true;
        p->z -= p->knock_v;
        p->knock_frames--;
    } else {
        float step = 0.0f;
        if (in->held & BTN_UP) step += PLAYER_SPEED * DT;
        if (in->held & BTN_DOWN) step -= PLAYER_SPEED * DT;
        p->z += step;
        if (step > 0.0f) {
            float lim = block_limit(p, dp);
            if (lim < prev) lim = prev;     /* never pushed backwards by a block */
            if (p->z > lim) p->z = lim;
        }
    }
    /* Holding down never goes below the floor, but the floor never pushes the player forward. */
    if (p->z < prev && !(p->knock_frames > 0 || knocked)) {
        float lo = floor_z < prev ? floor_z : prev;
        if (p->z < lo) p->z = lo;
    }
    if (p->z < hard) p->z = hard;

    for (i = 0; i < h->ndoors; i++) {
        Door* d = &h->doors[i];
        float thr = d->z + DOOR_LEN * 0.5f;
        bool pending;
        if (!(prev < thr && p->z >= thr)) continue;
        if (d->state == DOOR_CLOSED_DONE) continue;
        pending = d->remaining > 0 || door_has_emerging(dp, i);
        if (pending) {
            p->z = thr;
            p->knock_frames = KNOCKBACK_FRAMES;
            p->knock_v = KNOCKBACK_DIST / (float)KNOCKBACK_FRAMES;
            d->spawn_timer = 0;
            if (d->state == DOOR_CLOSED_FULL) d->state = DOOR_OPEN;
            ev |= PEV_DOOR_SLAM;
        } else {
            d->state = DOOR_CLOSED_DONE;
            h->doors_emptied++;
            if (d->z > p->min_z) p->min_z = d->z;
            ev |= PEV_DOOR_CLOSED;
        }
    }
    return ev;
}
