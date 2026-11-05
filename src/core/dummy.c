#include <math.h>
#include <string.h>
#include "dummy.h"
#include "tuning.h"

static const float k_height[DS_COUNT] = { DUMMY_H_SMALL, DUMMY_H_MEDIUM, DUMMY_H_LARGE };
static const float k_radius[DS_COUNT] = { DUMMY_RADIUS_SMALL, DUMMY_RADIUS_MEDIUM, DUMMY_RADIUS_LARGE };
static const float k_speed[DS_COUNT] = { DUMMY_SPEED_SMALL, DUMMY_SPEED_MEDIUM, DUMMY_SPEED_LARGE };
static const float k_hp[DS_COUNT] = { DUMMY_HP_SMALL, DUMMY_HP_MEDIUM, DUMMY_HP_LARGE };
static const int k_windup[DS_COUNT] = { DUMMY_WINDUP_SMALL, DUMMY_WINDUP_MEDIUM, DUMMY_WINDUP_LARGE };
static const int k_damage[DS_COUNT] = { DUMMY_DMG_SMALL, DUMMY_DMG_MEDIUM, DUMMY_DMG_LARGE };

float dummy_height(DummySize s) { return k_height[s]; }
float dummy_radius(DummySize s) { return k_radius[s]; }
float dummy_speed(DummySize s) { return k_speed[s]; }
float dummy_max_hp(DummySize s) { return k_hp[s]; }
int dummy_windup_frames(DummySize s) { return k_windup[s]; }
int dummy_damage(DummySize s) { return k_damage[s]; }

int dummy_live_count(const DummyPool* p) {
    int i, n = 0;
    for (i = 0; i < MAX_DUMMIES; i++)
        if (p->d[i].alive && p->d[i].state != DST_RAGDOLL && p->d[i].state != DST_DEAD) n++;
    return n;
}

int dummy_spawn(DummyPool* p, DummySize size, float x, float z, int door) {
    int i;
    for (i = 0; i < MAX_DUMMIES; i++) {
        Dummy* d = &p->d[i];
        if (d->alive) continue;
        memset(d, 0, sizeof(*d));
        d->alive = true;
        d->size = size;
        d->state = DST_EMERGING;
        d->timer = DUMMY_EMERGE_FRAMES;
        d->x = x;
        d->z = z;
        d->max_hp = dummy_max_hp(size) * (1.0f + DUMMY_HP_PER_100M * z / 100.0f);
        d->hp = d->max_hp;
        d->door = door;
        d->ragdoll = -1;
        return i;
    }
    return -1;
}

static bool separable(const Dummy* d) {
    return d->alive && d->state != DST_RAGDOLL && d->state != DST_DEAD;
}

static void clamp_dummy(Dummy* d, float player_z) {
    float r = dummy_radius(d->size);
    float lim = HALL_HALF_W - r;
    float min_z = player_z + PLAYER_RADIUS + r;
    if (d->x > lim) d->x = lim;
    if (d->x < -lim) d->x = -lim;
    if (d->z < min_z) d->z = min_z;
}

void dummy_update_all(DummyPool* p, float player_z, float player_x, int* out_damage_to_player) {
    int i, j, it, damage = 0;

    for (i = 0; i < MAX_DUMMIES; i++) {
        Dummy* d = &p->d[i];
        float r;
        if (!d->alive) continue;
        if (d->reaction_t > 0 && --d->reaction_t == 0) d->reaction = 0;
        r = dummy_radius(d->size);
        switch (d->state) {
        case DST_EMERGING:
            if (--d->timer <= 0) { d->state = DST_WALK; d->timer = 0; }
            break;
        case DST_WALK: {
            float dx = player_x - d->x;
            float dz = player_z + DUMMY_TARGET_Z_OFFSET - d->z;
            float dist = sqrtf(dx * dx + dz * dz);
            float step = dummy_speed(d->size) * DT;
            if (dist > 1e-6f) {
                if (step > dist) step = dist;
                d->x += dx / dist * step;
                d->z += dz / dist * step;
            }
            clamp_dummy(d, player_z);
            if (d->timer > 0) {
                d->timer--;
            } else if (d->z - player_z <= PLAYER_RADIUS + r + DUMMY_PUNCH_REACH) {
                d->state = DST_WINDUP;
                d->timer = dummy_windup_frames(d->size);
            }
            break;
        }
        case DST_WINDUP:
            if (--d->timer <= 0) {
                d->state = DST_PUNCH;
                d->timer = DUMMY_PUNCH_FRAMES;
                damage += dummy_damage(d->size);
            }
            break;
        case DST_PUNCH:
            if (--d->timer <= 0) { d->state = DST_WALK; d->timer = DUMMY_PUNCH_COOLDOWN; }
            break;
        case DST_STAGGER:
            if (--d->timer <= 0) { d->state = DST_WALK; d->timer = 0; }
            break;
        case DST_RAGDOLL:
        case DST_DEAD:
            break;
        }
    }

    /* Pairwise separation: push both apart equally along x and z. */
    for (it = 0; it < DUMMY_SEPARATION_ITERS; it++) {
        for (i = 0; i < MAX_DUMMIES; i++) {
            Dummy* a = &p->d[i];
            if (!separable(a)) continue;
            for (j = i + 1; j < MAX_DUMMIES; j++) {
                Dummy* b = &p->d[j];
                float dx, dz, dist, min_d, push, nx, nz;
                if (!separable(b)) continue;
                dx = b->x - a->x;
                dz = b->z - a->z;
                dist = sqrtf(dx * dx + dz * dz);
                min_d = dummy_radius(a->size) + dummy_radius(b->size);
                if (dist >= min_d) continue;
                if (dist < 1e-4f) { nx = 1.0f; nz = 0.0f; dist = 0.0f; }
                else { nx = dx / dist; nz = dz / dist; }
                /* EMERGING dummies are immovable obstacles: the other one takes the whole push. */
                if (a->state == DST_EMERGING && b->state == DST_EMERGING) continue;
                push = min_d - dist;
                if (a->state == DST_EMERGING) {
                    b->x += nx * push; b->z += nz * push;
                } else if (b->state == DST_EMERGING) {
                    a->x -= nx * push; a->z -= nz * push;
                } else {
                    push *= 0.5f;
                    a->x -= nx * push; a->z -= nz * push;
                    b->x += nx * push; b->z += nz * push;
                }
            }
        }
        for (i = 0; i < MAX_DUMMIES; i++)
            if (separable(&p->d[i]) && p->d[i].state != DST_EMERGING) clamp_dummy(&p->d[i], player_z);
    }

    *out_damage_to_player = damage;
}
