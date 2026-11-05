#include <math.h>
#include <string.h>
#include "ragdoll.h"
#include "combat.h"

static const struct { uint8_t a, b; } k_sticks[RAGDOLL_STICKS] = {
    { RP_BASE, RP_BODY_LO }, { RP_BODY_LO, RP_BODY_HI }, { RP_BODY_HI, RP_HEAD },
    { RP_BODY_HI, RP_ELBOW_L }, { RP_ELBOW_L, RP_FIST_L },
    { RP_BODY_HI, RP_ELBOW_R }, { RP_ELBOW_R, RP_FIST_R },
    { RP_BASE, RP_BODY_HI }, { RP_BODY_LO, RP_HEAD },   /* stiffeners */
};

static void free_dummy(DummyPool* dp, int idx) {
    if (!dp || idx < 0 || idx >= MAX_DUMMIES) return;
    dp->d[idx].state = DST_DEAD;
    dp->d[idx].alive = false;
}

int ragdoll_spawn(RagdollPool* p, DummyPool* dummies, int dummy, const DummyRig* rig, Vec3 launch) {
    int i, slot = 0;
    Ragdoll* r;
    Dummy* d = &dummies->d[dummy];
    Vec2 rp[RP_COUNT];
    float m = dummy_height(d->size) / rig->height_px;
    Vec2 sh_mid;
    Vec3 spin;

    for (i = 0; i < RAGDOLL_MAX; i++) {
        if (!p->r[i].active) { slot = i; break; }
        if (p->r[i].born < p->r[slot].born) slot = i;
    }
    r = &p->r[slot];
    if (r->active) free_dummy(dummies, r->dummy);   /* recycled: its dummy must not dangle */
    memset(r, 0, sizeof(*r));
    r->active = true;
    r->size = d->size;
    r->hp = RAGDOLL_HP;
    r->dummy = dummy;
    r->born = ++p->serial;
    d->ragdoll = slot;

    /* Rest pose in rig px (y up is negative), from the anchors. */
    sh_mid.x = 0.5f * (rig->shoulder_l.x + rig->shoulder_r.x);
    sh_mid.y = 0.5f * (rig->shoulder_l.y + rig->shoulder_r.y);
    rp[RP_BASE].x = 0.0f; rp[RP_BASE].y = 0.0f;
    rp[RP_BODY_HI] = sh_mid;
    rp[RP_BODY_LO].x = 2.0f * rig->body_center.x - sh_mid.x;
    rp[RP_BODY_LO].y = 2.0f * rig->body_center.y - sh_mid.y;
    rp[RP_HEAD] = rig->face_center;
    rp[RP_ELBOW_L].x = rig->shoulder_l.x; rp[RP_ELBOW_L].y = rig->shoulder_l.y + rig->upper_arm_px;
    rp[RP_FIST_L].x = rp[RP_ELBOW_L].x;   rp[RP_FIST_L].y = rp[RP_ELBOW_L].y + rig->lower_arm_px;
    rp[RP_ELBOW_R].x = rig->shoulder_r.x; rp[RP_ELBOW_R].y = rig->shoulder_r.y + rig->upper_arm_px;
    rp[RP_FIST_R].x = rp[RP_ELBOW_R].x;   rp[RP_FIST_R].y = rp[RP_ELBOW_R].y + rig->lower_arm_px;

    {   /* head and base get opposite velocity, horizontal and perpendicular to the launch, so it tumbles */
        float hl = sqrtf(launch.x * launch.x + launch.z * launch.z);
        float sp = v3_len(launch) * RAGDOLL_LAUNCH_SPIN;
        spin.y = 0.0f;
        if (hl > 1e-4f) { spin.x = launch.z / hl * sp; spin.z = -launch.x / hl * sp; }   /* up x launch */
        else { spin.x = sp; spin.z = 0.0f; }
    }
    for (i = 0; i < RP_COUNT; i++) {
        float mul = (i == RP_HEAD) ? RAGDOLL_HEAD_VEL : (i == RP_FIST_L || i == RP_FIST_R) ? RAGDOLL_FIST_VEL : 1.0f;
        Vec3 v = v3_scale(launch, mul);
        if (i == RP_HEAD) v = v3_add(v, spin);
        if (i == RP_BASE) v = v3_sub(v, spin);
        r->p[i].x = d->x + rp[i].x * m;
        r->p[i].y = -rp[i].y * m;
        r->p[i].z = d->z + RAGDOLL_LEAN * r->p[i].y;   /* a slight lean so it topples instead of balancing */
        r->prev[i] = v3_sub(r->p[i], v3_scale(v, DT));
    }
    for (i = 0; i < RAGDOLL_STICKS; i++)
        r->rest[i] = v3_len(v3_sub(r->p[k_sticks[i].a], r->p[k_sticks[i].b]));
    return slot;
}

typedef struct {
    float max_impact;          /* biggest normal impact speed this step (m/s) */
    Vec3 normal;               /* surface normal of that impact */
    int nslam;                 /* wall hits above RAGDOLL_IMPACT_MIN, one per point */
    int slam_side[RP_COUNT];
    float slam_z[RP_COUNT];
} Contacts;

/* Clamps point i into the hallway box; with c set, also applies the velocity response and records impacts. */
static void collide(Ragdoll* r, int i, Contacts* c) {
    Vec3* p = &r->p[i];
    Vec3* pr = &r->prev[i];
    float v;
    if (!c) {   /* position-only clamp: sticks may push a point into a surface, velocity is left alone */
        p->x = clampf(p->x, -HALL_HALF_W, HALL_HALF_W);
        p->y = clampf(p->y, 0.0f, HALL_H);
        return;
    }
    if (p->x > HALL_HALF_W || p->x < -HALL_HALF_W) {
        float s = p->x > 0.0f ? 1.0f : -1.0f;
        v = (p->x - pr->x) * s / DT;                      /* speed into the wall */
        p->x = s * HALL_HALF_W;
        if (v > 0.0f) {
            float out = v < RAGDOLL_BOUNCE_MIN ? 0.0f : v * RAGDOLL_RESTITUTION;
            pr->y = p->y - (p->y - pr->y) * RAGDOLL_FRICTION;
            pr->z = p->z - (p->z - pr->z) * RAGDOLL_FRICTION;
            pr->x = p->x + s * out * DT;
            if (c) {
                if (v > c->max_impact) { c->max_impact = v; c->normal = (Vec3){ -s, 0.0f, 0.0f }; }
                if (v > RAGDOLL_IMPACT_MIN) { c->slam_side[c->nslam] = (int)s; c->slam_z[c->nslam] = p->z; c->nslam++; }
            }
        }
    }
    if (p->y < 0.0f) {
        v = (pr->y - p->y) / DT;
        p->y = 0.0f;
        if (v > 0.0f) {
            float out = v < RAGDOLL_BOUNCE_MIN ? 0.0f : v * RAGDOLL_RESTITUTION;
            pr->x = p->x - (p->x - pr->x) * RAGDOLL_FRICTION;
            pr->z = p->z - (p->z - pr->z) * RAGDOLL_FRICTION;
            pr->y = -out * DT;
            if (c && v > c->max_impact) { c->max_impact = v; c->normal = (Vec3){ 0.0f, 1.0f, 0.0f }; }
        }
    } else if (p->y > HALL_H) {
        v = (p->y - pr->y) / DT;
        p->y = HALL_H;
        if (v > 0.0f) {
            float out = v < RAGDOLL_BOUNCE_MIN ? 0.0f : v * RAGDOLL_RESTITUTION;
            pr->x = p->x - (p->x - pr->x) * RAGDOLL_FRICTION;
            pr->z = p->z - (p->z - pr->z) * RAGDOLL_FRICTION;
            pr->y = HALL_H + out * DT;
            if (c && v > c->max_impact) { c->max_impact = v; c->normal = (Vec3){ 0.0f, -1.0f, 0.0f }; }
        }
    }
}

static void solve_sticks(Ragdoll* r) {
    int it, s;
    for (it = 0; it < RAGDOLL_ITER; it++) {
        for (s = 0; s < RAGDOLL_STICKS; s++) {
            Vec3* a = &r->p[k_sticks[s].a];
            Vec3* b = &r->p[k_sticks[s].b];
            Vec3 d = v3_sub(*b, *a);
            float len = v3_len(d), k;
            if (len < 1e-6f) continue;
            k = 0.5f * (len - r->rest[s]) / len;
            *a = v3_add(*a, v3_scale(d, k));
            *b = v3_sub(*b, v3_scale(d, k));
        }
        for (s = 0; s < RP_COUNT; s++) collide(r, s, NULL);   /* keep inside; no velocity response */
    }
}

static Vec3 point_vel(const Ragdoll* r, int i) {
    return v3_scale(v3_sub(r->p[i], r->prev[i]), 1.0f / DT);
}

#define MAX_PENDING 16
typedef struct { int dummy; Vec3 launch; } Pending;

void ragdoll_step(RagdollPool* p, DummyPool* dummies, Hallway* hall, const DummyRig rigs[DS_COUNT],
                  RagdollStepOut* out) {
    Pending pend[MAX_PENDING];
    int npend = 0, ri, i, di, k;
    memset(out, 0, sizeof(*out));

    for (ri = 0; ri < RAGDOLL_MAX; ri++) {
        Ragdoll* r = &p->r[ri];
        Contacts c;
        float max_speed = 0.0f;
        if (!r->active) continue;
        for (di = 0; di < MAX_DUMMIES; di++) if (r->bowl_cd[di] > 0) r->bowl_cd[di]--;

        /* Verlet integration, with wall/floor response on the unconstrained motion. */
        memset(&c, 0, sizeof c);
        for (i = 0; i < RP_COUNT; i++) {
            Vec3 cur = r->p[i];
            Vec3 v = v3_sub(cur, r->prev[i]);
            r->prev[i] = cur;
            r->p[i].x = cur.x + v.x;
            r->p[i].y = cur.y + v.y - GRAVITY * DT * DT;
            r->p[i].z = cur.z + v.z;
            collide(r, i, &c);
        }
        solve_sticks(r);

        /* One impact is charged once: contact steps within the window only pay for the excess over what
           the window has already charged (fists, head and body reach the wall on different steps). */
        if (c.max_impact > RAGDOLL_IMPACT_MIN) {
            float ex = c.max_impact - RAGDOLL_IMPACT_MIN;
            if (ex > r->charged) { r->hp -= ex - r->charged; r->charged = ex; }
            r->charge_t = RAGDOLL_IMPACT_WINDOW;
        } else if (r->charge_t > 0 && --r->charge_t == 0) {
            r->charged = 0.0f;
        }

        /* Door slam: a hard wall hit inside an open door's span. */
        for (k = 0; hall && k < c.nslam; k++) {
            for (i = 0; i < hall->ndoors; i++) {
                Door* dr = &hall->doors[i];
                if (dr->state != DOOR_OPEN || dr->side != c.slam_side[k]) continue;
                if (c.slam_z[k] < dr->z || c.slam_z[k] > dr->z + DOOR_LEN) continue;
                dr->state = DOOR_CLOSED_FULL;
                dr->leaf_angle = 0.0f;
                for (di = 0; di < MAX_DUMMIES; di++) {
                    Dummy* d = &dummies->d[di];
                    if (!d->alive || d->state != DST_EMERGING || d->door != i) continue;
                    d->hp -= RAGDOLL_DOORSLAM_DMG;
                    d->reaction_t = 0;
                    if (d->hp <= 0.0f) {
                        d->state = DST_RAGDOLL; d->timer = 0; d->launch = (Vec3){ 0.0f, 0.0f, 0.0f };
                        if (npend < MAX_PENDING) { pend[npend].dummy = di; pend[npend].launch = d->launch; npend++; }
                    } else {
                        d->state = DST_STAGGER; d->timer = DUMMY_STAGGER_FRAMES;
                    }
                }
            }
        }

        /* Bowling: the fastest point inside each live dummy's cylinder hits it. */
        for (di = 0; di < MAX_DUMMIES; di++) {
            Dummy* d = &dummies->d[di];
            float rad, h, best = 0.0f;
            int bi = -1;
            if (!d->alive || d->state == DST_RAGDOLL || d->state == DST_DEAD || d->state == DST_EMERGING) continue;
            if (r->bowl_cd[di] > 0) continue;
            rad = dummy_radius(d->size); h = dummy_height(d->size);
            for (i = 0; i < RP_COUNT; i++) {
                float dx = r->p[i].x - d->x, dz = r->p[i].z - d->z, sp;
                if (dx * dx + dz * dz > rad * rad || r->p[i].y > h) continue;
                sp = v3_len(point_vel(r, i));
                if (sp > RAGDOLL_BOWL_MIN && sp > best) { best = sp; bi = i; }
            }
            if (bi < 0) continue;
            {
                Vec3 v = point_vel(r, bi);
                float lxz = sqrtf(v.x * v.x + v.z * v.z);
                r->bowl_cd[di] = RAGDOLL_BOWL_COOLDOWN;
                out->bowl_hits++;
                d->hp -= RAGDOLL_BOWL_DMG * best;
                d->reaction = r->p[bi].x < d->x ? RX_FROM_LEFT : RX_FROM_RIGHT;
                d->reaction_t = REACTION_FRAMES;
                if (lxz > 1e-4f) {
                    float lim = HALL_HALF_W - rad;
                    d->x = clampf(d->x + v.x / lxz * RAGDOLL_BOWL_PUSH, -lim, lim);
                    d->z += v.z / lxz * RAGDOLL_BOWL_PUSH;
                }
                if (d->hp <= 0.0f) {
                    d->state = DST_RAGDOLL; d->timer = 0;
                    d->launch = v3_scale(v, RAGDOLL_BOWL_LAUNCH);
                    if (npend < MAX_PENDING) { pend[npend].dummy = di; pend[npend].launch = d->launch; npend++; }
                } else {
                    d->state = DST_STAGGER; d->timer = DUMMY_STAGGER_FRAMES;
                }
            }
        }

        /* Burst, else settle. */
        if (r->hp <= 0.0f) {
            if (out->burst_count < 6) {
                out->burst_idx[out->burst_count] = ri;
                out->burst_at[out->burst_count] = r->p[RP_BODY_LO];
                out->burst_n[out->burst_count] = c.normal;
                out->burst_count++;
            }
            r->active = false;
            free_dummy(dummies, r->dummy);
            continue;
        }
        for (i = 0; i < RP_COUNT; i++) {
            float sp = v3_len(point_vel(r, i));
            if (sp > max_speed) max_speed = sp;
        }
        if (max_speed < RAGDOLL_SETTLE_SPEED) r->still_frames++; else r->still_frames = 0;
        if (r->still_frames >= RAGDOLL_REST_FRAMES) {
            r->active = false;
            free_dummy(dummies, r->dummy);
        }
    }

    /* A freed dummy slot may be reused: no ragdoll keeps a cooldown for it. */
    for (di = 0; di < MAX_DUMMIES; di++) {
        if (dummies->d[di].alive) continue;
        for (ri = 0; ri < RAGDOLL_MAX; ri++) p->r[ri].bowl_cd[di] = 0;
    }

    /* Spawned after the loop so a recycled slot can never be one being stepped. */
    for (i = 0; i < npend; i++) {
        Dummy* d = &dummies->d[pend[i].dummy];
        if (d->alive && d->state == DST_RAGDOLL && d->ragdoll < 0)
            ragdoll_spawn(p, dummies, pend[i].dummy, &rigs[d->size], pend[i].launch);
    }
}

/* ---- drawing ---- */

#define RAD_TO_DEG 57.29578f

typedef struct { float x, y, ppm; } SP;

/* Bitmap angle that turns an image's "up" (0,-1) along a -> b on screen. */
static float up_angle(SP a, SP b) { return atan2f(b.x - a.x, a.y - b.y) * RAD_TO_DEG; }
/* Bitmap angle that turns an image's "down" (0,1), as a hanging arm, along a -> b on screen. */
static float down_angle(SP a, SP b) { return atan2f(a.x - b.x, b.y - a.y) * RAD_TO_DEG; }
static float dist2(SP a, SP b) { return sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)); }

/* A hanging limb image from a to b: top-centre on a, stretched to the projected length. */
static void limb(RenderList* rl, int image, SP a, SP b, float rest_px, float s) {
    float sy = dist2(a, b) / rest_px;
    rl_bitmap(rl, image, a.x, a.y, down_angle(a, b), 0.5f, 0.0f, s, sy < 0.02f ? 0.02f : sy, false);
}

void draw_ragdoll(RenderList* rl, const Camera* cam, const Ragdoll* r, const DummyRig* rig) {
    SP s[RP_COUNT];
    bool ok[RP_COUNT];
    float m = dummy_height(r->size) / rig->height_px;   /* metres per rig px */
    float scale[RP_COUNT], sb, ang, sy, rest_px, tilt;
    int i, sz = (int)r->size;
    SP mid, shl, shr;
    bool body_ok;

    /* A point that is behind the near plane or beyond the far plane only drops the parts that need it. */
    for (i = 0; i < RP_COUNT; i++) {
        ok[i] = r->p[i].z - cam->player_z <= FAR_Z &&
                camera_project(cam, r->p[i], &s[i].x, &s[i].y, &s[i].ppm);
        scale[i] = ok[i] ? s[i].ppm * m : 0.0f;   /* screen px per rig px at this point */
    }
    body_ok = ok[RP_BODY_LO] && ok[RP_BODY_HI];
    sb = 0.5f * (scale[RP_BODY_LO] + scale[RP_BODY_HI]);

    if (ok[RP_BASE] && ok[RP_BODY_LO] && r->p[RP_BASE].y <= RAGDOLL_BASE_HIDE_Y) {
        ang = up_angle(s[RP_BASE], s[RP_BODY_LO]);
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_BASE), s[RP_BASE].x, s[RP_BASE].y, ang, 0.5f, 0.5f,
                  scale[RP_BASE], scale[RP_BASE], false);
    }

    /* Torso: centred between the lower and upper body, stretched along its projected length. */
    ang = 0.0f;
    if (body_ok) {
        mid.x = 0.5f * (s[RP_BODY_LO].x + s[RP_BODY_HI].x);
        mid.y = 0.5f * (s[RP_BODY_LO].y + s[RP_BODY_HI].y);
        ang = up_angle(s[RP_BODY_LO], s[RP_BODY_HI]);
        rest_px = r->rest[1] / m;
        sy = dist2(s[RP_BODY_LO], s[RP_BODY_HI]) / rest_px;
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_BODY), mid.x, mid.y, ang, 0.5f, 0.5f, sb, sy < 0.02f ? 0.02f : sy, false);
    }

    /* Arms hang from the shoulders, which sit either side of the upper body along its width axis. */
    if (ok[RP_BODY_HI]) {
        tilt = ang / RAD_TO_DEG;
        shl.x = s[RP_BODY_HI].x + cosf(tilt) * rig->shoulder_l.x * scale[RP_BODY_HI];
        shl.y = s[RP_BODY_HI].y + sinf(tilt) * rig->shoulder_l.x * scale[RP_BODY_HI];
        shr.x = s[RP_BODY_HI].x + cosf(tilt) * rig->shoulder_r.x * scale[RP_BODY_HI];
        shr.y = s[RP_BODY_HI].y + sinf(tilt) * rig->shoulder_r.x * scale[RP_BODY_HI];
        shl.ppm = shr.ppm = s[RP_BODY_HI].ppm;
        if (ok[RP_ELBOW_L])
            limb(rl, img_dummy((DummySize)sz, PART_ARM_UPPER), shl, s[RP_ELBOW_L], rig->upper_arm_px, scale[RP_ELBOW_L]);
        if (ok[RP_ELBOW_R])
            limb(rl, img_dummy((DummySize)sz, PART_ARM_UPPER), shr, s[RP_ELBOW_R], rig->upper_arm_px, scale[RP_ELBOW_R]);
    }
    if (ok[RP_ELBOW_L] && ok[RP_FIST_L]) {
        limb(rl, img_dummy((DummySize)sz, PART_ARM_LOWER), s[RP_ELBOW_L], s[RP_FIST_L], rig->lower_arm_px, scale[RP_ELBOW_L]);
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_FIST), s[RP_FIST_L].x, s[RP_FIST_L].y,
                  down_angle(s[RP_ELBOW_L], s[RP_FIST_L]), 0.5f, 0.5f, scale[RP_FIST_L], scale[RP_FIST_L], false);
    }
    if (ok[RP_ELBOW_R] && ok[RP_FIST_R]) {
        limb(rl, img_dummy((DummySize)sz, PART_ARM_LOWER), s[RP_ELBOW_R], s[RP_FIST_R], rig->lower_arm_px, scale[RP_ELBOW_R]);
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_FIST), s[RP_FIST_R].x, s[RP_FIST_R].y,
                  down_angle(s[RP_ELBOW_R], s[RP_FIST_R]), 0.5f, 0.5f, scale[RP_FIST_R], scale[RP_FIST_R], false);
    }

    /* Head and its knocked-out face, centred on the head point, turned along body_hi -> head. */
    if (ok[RP_BODY_HI] && ok[RP_HEAD]) {
        ang = up_angle(s[RP_BODY_HI], s[RP_HEAD]);
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_HEAD), s[RP_HEAD].x, s[RP_HEAD].y, ang, 0.5f, 0.5f,
                  scale[RP_HEAD], scale[RP_HEAD], false);
        rl_bitmap(rl, img_dummy((DummySize)sz, PART_FACE_KO), s[RP_HEAD].x, s[RP_HEAD].y, ang, 0.5f, 0.5f,
                  scale[RP_HEAD], scale[RP_HEAD], false);
    }
}
