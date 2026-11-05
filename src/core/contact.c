#include "contact.h"
#include <math.h>
#include "pose.h"
#include "tuning.h"

#define DEG2RAD (3.14159265f / 180.0f)
#define LAUNCH_SIDE 0.5f
#define LAUNCH_UP_MIN 0.3f
#define LAUNCH_FORWARD 0.9f
#define STRUCK_RELEASE_DEG 180.0f   /* a struck dummy is let go after half a turn: the next pass is a new turn */

/* Weapon.inside has one bit per pool slot. */
typedef char contact_inside_fits_pool[(MAX_DUMMIES <= 16) ? 1 : -1];

typedef struct { bool ok; ScreenBox head, body; } Candidate;
typedef struct { bool head; float sx, sy, r; } Contact;
/* pass: the head's first overlap from the entry on, else the body at entry (a hit that carries on).
   at_entry: the head if it overlaps at the entry sample, else the body there (a hit that bounces). */
typedef struct { int dummy, k; Contact pass, at_entry; } Entry;

void weapon_segment(const Weapon* wp, const WeaponRig* wr, float angle_deg,
                    float* gx, float* gy, float* tx, float* ty) {
    float s = rsin_deg(angle_deg), c = rcos_deg(angle_deg), px = weapon_pivot_x(wp), rt = weapon_tip_radius(wr);
    *gx = px + WEAPON_SWEEP_RADIUS * s;
    *gy = WEAPON_PIVOT_Y - WEAPON_SWEEP_RADIUS * c;
    *tx = px + rt * s;
    *ty = WEAPON_PIVOT_Y - rt * c;
}

float weapon_tip_radius(const WeaponRig* wr) {
    float dx = wr->tip.x - wr->grip.x, dy = wr->tip.y - wr->grip.y;
    return WEAPON_SWEEP_RADIUS + sqrtf(dx * dx + dy * dy);
}

float contact_momentum(const WeaponDef* w, float vel_deg, float r_px) {
    return w->mass * fabsf(vel_deg) * DEG2RAD * r_px;
}

float dummy_resist(DummySize s) {
    if (s == DS_SMALL) return DUMMY_RESIST_SMALL;
    if (s == DS_MEDIUM) return DUMMY_RESIST_MEDIUM;
    return DUMMY_RESIST_LARGE;
}

bool contact_heavy(const WeaponDef* w, float m) {
    return w->mass >= HIT_HEAVY_MASS && m >= HIT_HEAVY_MOMENTUM;
}

/* Screen tangent of the sweep at angle_deg in the direction of vel. */
static void tangent(float angle_deg, float vel, float* tx, float* ty) {
    float sg = vel < 0.0f ? -1.0f : 1.0f;
    *tx = rcos_deg(angle_deg) * sg;
    *ty = rsin_deg(angle_deg) * sg;
}

ReactionType contact_reaction(float angle_deg, float vel) {
    float tx, ty;
    tangent(angle_deg, vel, &tx, &ty);
    if (fabsf(tx) >= fabsf(ty)) return tx > 0.0f ? RX_FROM_LEFT : RX_FROM_RIGHT;
    return ty < 0.0f ? RX_UPWARD : RX_DOWNWARD;
}

static Vec3 launch_vector(const WeaponDef* w, float angle_deg, float vel, float k) {
    Vec3 v;
    float tx, ty;
    tangent(angle_deg, vel, &tx, &ty);
    v.x = tx * LAUNCH_SIDE;
    v.y = -ty > LAUNCH_UP_MIN ? -ty : LAUNCH_UP_MIN;
    v.z = LAUNCH_FORWARD;
    return v3_scale(v, w->launch * k / v3_len(v));
}

/* Liang-Barsky: clips (x0,y0)-(x1,y1) to b. On overlap, writes the midpoint of the clipped part. */
static bool clip_segment(float x0, float y0, float x1, float y1, const ScreenBox* b, float* cx, float* cy) {
    float dx = x1 - x0, dy = y1 - y0, t0 = 0.0f, t1 = 1.0f, tm;
    float p[4], q[4];
    int i;
    p[0] = -dx; q[0] = x0 - b->x0;
    p[1] = dx;  q[1] = b->x1 - x0;
    p[2] = -dy; q[2] = y0 - b->y0;
    p[3] = dy;  q[3] = b->y1 - y0;
    for (i = 0; i < 4; i++) {
        if (p[i] == 0.0f) {
            if (q[i] < 0.0f) return false;
        } else {
            float r = q[i] / p[i];
            if (p[i] < 0.0f) {
                if (r > t1) return false;
                if (r > t0) t0 = r;
            } else {
                if (r < t0) return false;
                if (r < t1) t1 = r;
            }
        }
    }
    tm = 0.5f * (t0 + t1);
    *cx = x0 + dx * tm;
    *cy = y0 + dy * tm;
    return true;
}

static bool overlap(const Weapon* wp, const WeaponRig* wr, float angle_deg, const ScreenBox* b, float* cx, float* cy) {
    float gx, gy, tx, ty;
    weapon_segment(wp, wr, angle_deg, &gx, &gy, &tx, &ty);
    return clip_segment(gx, gy, tx, ty, b, cx, cy);
}

static bool overlap_any(const Weapon* wp, const WeaponRig* wr, float angle_deg, const Candidate* c) {
    float x, y;
    return overlap(wp, wr, angle_deg, &c->head, &x, &y) || overlap(wp, wr, angle_deg, &c->body, &x, &y);
}

/* The old combat gate plus projected boxes, with the pose draw_world uses. */
static void gather(const WeaponDef* w, const Camera* cam, const DummyPool* pool, const DummyRig rigs[DS_COUNT],
                   int frame, Candidate cand[MAX_DUMMIES]) {
    int i;
    for (i = 0; i < MAX_DUMMIES; i++) {
        const Dummy* d = &pool->d[i];
        DummyPose pose;
        float dz = d->z - cam->player_z;
        cand[i].ok = false;
        if (!d->alive || d->state == DST_EMERGING || d->state == DST_RAGDOLL || d->state == DST_DEAD) continue;
        if (!(dz > 0.0f && dz <= w->reach)) continue;
        if (d->reaction_t > 0) pose_for_reaction((ReactionType)d->reaction, (float)d->reaction_t / (float)REACTION_FRAMES, d, frame, &pose);
        else pose_for_state(d, frame, &pose);
        cand[i].ok = dummy_hit_boxes(cam, d, &rigs[d->size], &pose, &cand[i].head, &cand[i].body);
    }
}

static uint16_t inside_mask(const Weapon* wp, const WeaponRig* wr, const Candidate cand[MAX_DUMMIES]) {
    uint16_t m = 0;
    int i;
    for (i = 0; i < MAX_DUMMIES; i++)
        if (cand[i].ok && overlap_any(wp, wr, wp->angle, &cand[i])) m = (uint16_t)(m | (1u << i));
    return m;
}

/* True if the weapon overlaps c anywhere along the move from a0 to a1 (sampled as contact_sweep samples). */
static bool move_overlaps(const Weapon* wp, const WeaponRig* wr, float a0, float a1, const Candidate* c) {
    float da = a1 - a0;
    int k, n = (int)ceilf(fabsf(da) / WEAPON_SWEEP_STEP_DEG);
    for (k = 0; k <= n; k++)
        if (overlap_any(wp, wr, n > 0 ? a0 + da * (float)k / (float)n : a0, c)) return true;
    return false;
}

/* One hit per pass. A struck dummy is pushed back and switches to its reaction pose, so its boxes shrink and
   can drop off the weapon, or move ahead of it, mid-stroke; neither is the weapon leaving it. So a struck dummy
   counts as inside until the weapon reverses (a return stroke or a bounce), or moves out of its boxes, or has
   turned half a circle since the strike (the next crank turn), or the dummy is no longer a target.
   Runs before the entry search, so a return stroke can hit at once. */
static void release_on_reverse(Weapon* wp, const WeaponRig* wr, const Candidate cand[MAX_DUMMIES], float da) {
    int i;
    bool reversed = da != 0.0f && (da > 0.0f) != (wp->struck_dir > 0.0f);
    for (i = 0; i < MAX_DUMMIES; i++) {
        uint16_t bit = (uint16_t)(1u << i);
        if (!(wp->struck & bit)) continue;
        if (!cand[i].ok || reversed) {
            wp->struck = (uint16_t)(wp->struck & ~bit);
            if (!cand[i].ok || !overlap_any(wp, wr, wp->prev_angle, &cand[i])) wp->inside = (uint16_t)(wp->inside & ~bit);
        }
    }
}

/* After the hits, for dummies struck on an earlier frame: released once this frame's move takes the weapon out
   of their boxes, or the weapon has turned STRUCK_RELEASE_DEG since the strike. */
static void release_on_exit(Weapon* wp, const WeaponRig* wr, const Candidate cand[MAX_DUMMIES], uint16_t keep) {
    int i;
    float da = wp->angle - wp->prev_angle;
    if (!wp->struck) return;
    wp->struck_travel += fabsf(da);
    for (i = 0; i < MAX_DUMMIES; i++) {
        uint16_t bit = (uint16_t)(1u << i);
        if (!(wp->struck & bit) || (keep & bit)) continue;
        if (!cand[i].ok || wp->struck_travel >= STRUCK_RELEASE_DEG ||
            (da != 0.0f && move_overlaps(wp, wr, wp->prev_angle, wp->angle, &cand[i]) &&
             !overlap_any(wp, wr, wp->angle, &cand[i])))
            wp->struck = (uint16_t)(wp->struck & ~bit);
    }
}

static void set_contact(const Weapon* wp, Contact* c, bool head, float x, float y) {
    float dx = x - weapon_pivot_x(wp), dy = y - WEAPON_PIVOT_Y;
    c->head = head;
    c->sx = x;
    c->sy = y;
    c->r = sqrtf(dx * dx + dy * dy);
}

/* Finds dummy i's entry along the samples; false if it is not entered this frame. */
static bool find_entry(const Weapon* wp, const WeaponRig* wr, const Candidate* c, int i, int n, Entry* e) {
    float a0 = wp->prev_angle, da = wp->angle - wp->prev_angle, x = 0.0f, y = 0.0f;
    int k, entry = -1;
    if (overlap_any(wp, wr, a0, c)) return false;
    for (k = 1; k <= n && entry < 0; k++)
        if (overlap_any(wp, wr, a0 + da * (float)k / (float)n, c)) entry = k;
    if (entry < 0) return false;
    e->dummy = i;
    e->k = entry;
    if (overlap(wp, wr, a0 + da * (float)entry / (float)n, &c->head, &x, &y)) {
        set_contact(wp, &e->at_entry, true, x, y);
        e->pass = e->at_entry;
        return true;
    }
    overlap(wp, wr, a0 + da * (float)entry / (float)n, &c->body, &x, &y);
    set_contact(wp, &e->at_entry, false, x, y);
    e->pass = e->at_entry;
    for (k = entry + 1; k <= n; k++) {
        if (overlap(wp, wr, a0 + da * (float)k / (float)n, &c->head, &x, &y)) {
            set_contact(wp, &e->pass, true, x, y);
            break;
        }
    }
    return true;
}

static float hit_damage(const WeaponDef* w, const Contact* c, float m) {
    return m * w->power * (c->head ? HEAD_MULT : 1.0f) * DAMAGE_PER_MOMENTUM;
}

static void apply_hit(const WeaponDef* w, Dummy* d, int dummy, const Contact* c, float a_k, float vel, float m,
                      HitResult* h) {
    float k = clampf(m / MOMENTUM_REF, HIT_SCALE_MIN, HIT_SCALE_MAX);
    h->dummy = dummy;
    h->head = c->head;
    h->damage = hit_damage(w, c, m);
    h->rx = contact_reaction(a_k, vel);
    h->launch.x = 0.0f; h->launch.y = 0.0f; h->launch.z = 0.0f;
    h->sx = c->sx;
    h->sy = c->sy;
    h->m = m;

    d->hp -= h->damage;
    d->reaction = (int)h->rx;
    d->reaction_t = REACTION_FRAMES;
    d->z += DUMMY_HIT_PUSHBACK * k;
    h->knockout = d->hp <= 0.0f;
    if (h->knockout) {
        h->launch = launch_vector(w, a_k, vel, k);
        d->launch = h->launch;
        d->state = DST_RAGDOLL;
        d->timer = 0;
    } else {
        d->state = DST_STAGGER;
        d->timer = DUMMY_STAGGER_FRAMES;
    }
}

int contact_sweep(Weapon* wp, const WeaponDef* w, const WeaponRig* wr, const Camera* cam,
                  DummyPool* pool, const DummyRig rigs[DS_COUNT], int frame,
                  HitResult* out, int max_out) {
    Candidate cand[MAX_DUMMIES];
    Entry ent[MAX_DUMMIES];
    float da = wp->angle - wp->prev_angle;
    int i, j, n, ne = 0, nh = 0;
    uint16_t hit_now = 0;

    gather(w, cam, pool, rigs, frame, cand);
    release_on_reverse(wp, wr, cand, da);
    if (!wp->swept || da == 0.0f) {
        release_on_exit(wp, wr, cand, 0);
        wp->inside = (uint16_t)(inside_mask(wp, wr, cand) | wp->struck);
        return 0;
    }

    n = (int)ceilf(fabsf(da) / WEAPON_SWEEP_STEP_DEG);
    if (n < 1) n = 1;
    for (i = 0; i < MAX_DUMMIES; i++) {
        Entry e;
        if (!cand[i].ok || (wp->inside & (1u << i))) continue;
        if (!find_entry(wp, wr, &cand[i], i, n, &e)) continue;
        /* Insertion by k; dummies arrive in index order, so ties stay in index order. */
        for (j = ne; j > 0 && ent[j - 1].k > e.k; j--) ent[j] = ent[j - 1];
        ent[j] = e;
        ne++;
    }

    for (i = 0; i < ne && nh < max_out && wp->attached; i++) {
        const Entry* e = &ent[i];
        Dummy* d = &pool->d[e->dummy];
        float a_k = wp->prev_angle + da * (float)e->k / (float)n;
        const Contact* c = &e->pass;
        float m = contact_momentum(w, wp->vel, c->r);
        float R = dummy_resist(d->size);
        HitResult* h = &out[nh++];
        /* A hit that will bounce stops at the entry sample, so it scores only what the weapon reached there. */
        if (m < R && d->hp - hit_damage(w, c, m) > 0.0f) {
            c = &e->at_entry;
            m = contact_momentum(w, wp->vel, c->r);
        }
        apply_hit(w, d, e->dummy, c, a_k, wp->vel, m, h);
        hit_now = (uint16_t)(hit_now | (1u << e->dummy));
        if (m >= R || h->knockout) {
            float keep = m > 0.0f ? 1.0f - R / m : 0.0f;
            wp->vel *= keep > 0.0f ? keep : 0.0f;
        } else {
            weapon_detach(wp, a_k, -wp->vel * WEAPON_BOUNCE_RESTITUTION);
        }
    }

    gather(w, cam, pool, rigs, frame, cand);
    release_on_exit(wp, wr, cand, hit_now);
    for (i = 0; i < MAX_DUMMIES; i++)   /* latch this frame's hits; a knockout is no longer a target */
        if ((hit_now & (1u << i)) && cand[i].ok) wp->struck = (uint16_t)(wp->struck | (1u << i));
    if (hit_now) {
        wp->struck_dir = da > 0.0f ? 1.0f : -1.0f;
        wp->struck_travel = 0.0f;
    }
    wp->inside = (uint16_t)(inside_mask(wp, wr, cand) | wp->struck);
    return nh;
}
