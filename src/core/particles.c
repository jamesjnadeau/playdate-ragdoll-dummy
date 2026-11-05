#include "particles.h"
#include "rig.h"

static Particle* slot(ParticlePool* pp) {
    Particle* q = &pp->p[pp->next];
    pp->next = (pp->next + 1) % PARTICLE_MAX;
    *q = (Particle){ 0 };
    return q;
}

static float rnd(Rng* r, float lo, float hi) { return lo + (hi - lo) * rng_f01(r); }

/* A unit vector in the hemisphere around n (n unit), biased toward n so sprays leave the surface. */
static Vec3 hemi_dir(Rng* r, Vec3 n) {
    Vec3 t = { rnd(r, -1.0f, 1.0f), rnd(r, -1.0f, 1.0f), rnd(r, -1.0f, 1.0f) };
    float a = rnd(r, 0.25f, 1.0f), l;
    Vec3 d;
    t = v3_sub(t, v3_scale(n, v3_dot(t, n)));          /* tangential part */
    d = v3_add(v3_scale(n, a), t);
    l = v3_len(d);
    if (l < 1e-5f) return n;
    return v3_scale(d, 1.0f / l);
}

static void spawn(ParticlePool* pp, Rng* r, ParticleKind k, Vec3 at, Vec3 v, int life) {
    Particle* q = slot(pp);
    q->active = true; q->kind = k;
    q->variant = (uint8_t)rng_range(r, 0, 3);
    q->p = at; q->v = v;
    q->angle = rnd(r, 0.0f, 360.0f);
    q->spin = k == PK_SCRAP ? rnd(r, -PARTICLE_SPIN_MAX, PARTICLE_SPIN_MAX) : 0.0f;
    q->life = life;
}

void particles_burst(ParticlePool* pp, Rng* rng, Vec3 at, Vec3 n) {
    float l = v3_len(n);
    int i;
    n = l > 1e-5f ? v3_scale(n, 1.0f / l) : (Vec3){ 0.0f, 1.0f, 0.0f };
    for (i = 0; i < BURST_PUFFS; i++)
        spawn(pp, rng, PK_PUFF, at, v3_scale(hemi_dir(rng, n), rnd(rng, PARTICLE_PUFF_SPEED_MIN, PARTICLE_PUFF_SPEED_MAX)),
              PARTICLE_LIFE_FRAMES);
    for (i = 0; i < BURST_SCRAPS; i++)
        spawn(pp, rng, PK_SCRAP, at, v3_scale(hemi_dir(rng, n), rnd(rng, PARTICLE_SCRAP_SPEED_MIN, PARTICLE_SCRAP_SPEED_MAX)),
              PARTICLE_LIFE_FRAMES);
    for (i = 0; i < BURST_FLOOR_PUFFS; i++) {
        Vec3 v = { n.x * 0.5f + rnd(rng, -PARTICLE_FLOOR_SPREAD, PARTICLE_FLOOR_SPREAD),
                   -PARTICLE_FLOOR_DOWN * rng_f01(rng),
                   rnd(rng, -PARTICLE_FLOOR_SPREAD, PARTICLE_FLOOR_SPREAD) };
        spawn(pp, rng, PK_PUFF, at, v, PARTICLE_LIFE_FRAMES);
    }
}

void particles_teeth(ParticlePool* pp, Rng* rng, Vec3 at) {
    int i;
    for (i = 0; i < TOOTH_COUNT; i++) {
        Vec3 v = { (i == 0 ? -1.0f : 1.0f) * rnd(rng, 0.5f, 1.0f) * TOOTH_OUT_SPEED,
                   TOOTH_UP_SPEED * rnd(rng, 0.7f, 1.0f),
                   rnd(rng, -0.3f, 0.3f) };
        spawn(pp, rng, PK_TOOTH, at, v, TOOTH_LIFE_FRAMES);
        pp->p[(pp->next + PARTICLE_MAX - 1) % PARTICLE_MAX].spin = rnd(rng, -PARTICLE_SPIN_MAX, PARTICLE_SPIN_MAX);
    }
}

void particles_step(ParticlePool* pp) {
    int i;
    const float lim = HALL_HALF_W - PARTICLE_WALL_INSET;
    for (i = 0; i < PARTICLE_MAX; i++) {
        Particle* q = &pp->p[i];
        if (!q->active) continue;
        if (--q->life <= 0) { q->active = false; continue; }
        if (q->settled) continue;
        q->v.y -= GRAVITY * DT;
        q->p = v3_add(q->p, v3_scale(q->v, DT));
        q->angle += q->spin;
        if (q->p.x > lim) { q->p.x = lim; q->v.x = 0.0f; }
        else if (q->p.x < -lim) { q->p.x = -lim; q->v.x = 0.0f; }
        if (q->p.y > HALL_H) { q->p.y = HALL_H; q->v.y = 0.0f; }
        if (q->p.y <= 0.0f) {
            q->p.y = 0.0f;
            if (q->kind != PK_PUFF && !q->bounced && q->v.y < -1.0f) {
                q->bounced = 1;
                q->v.y = -q->v.y * PARTICLE_BOUNCE;
                q->v.x *= 0.5f; q->v.z *= 0.5f;
                q->spin *= 0.5f;
            } else {
                q->settled = true;
                q->v = (Vec3){ 0.0f, 0.0f, 0.0f };
                q->spin = 0.0f;
            }
        }
    }
}

#define DEPTH_BUCKETS 64

static float size_m(ParticleKind k) {
    return k == PK_PUFF ? PUFF_SIZE_M : k == PK_SCRAP ? SCRAP_SIZE_M : TOOTH_SIZE_M;
}

void draw_particles(RenderList* rl, const Camera* cam, const ParticlePool* pp) {
    /* Counting sort by depth: far buckets first. Within a bucket (0.25 m) the order is arbitrary. */
    struct { float x, y, ppm; } sp[PARTICLE_MAX];
    bool vis[PARTICLE_MAX];
    uint8_t bucket[PARTICLE_MAX];
    int16_t order[PARTICLE_MAX];
    int start[DEPTH_BUCKETS + 1] = { 0 };
    int i, n = 0, skip;

    for (i = 0; i < PARTICLE_MAX; i++) {
        const Particle* q = &pp->p[i];
        float sx, sy, ppm, dz;
        int bk;
        vis[i] = false;
        if (!q->active) continue;
        dz = q->p.z - cam->player_z;
        if (dz > FAR_Z) continue;
        if (!camera_project(cam, q->p, &sx, &sy, &ppm)) continue;
        if (size_m(q->kind) * ppm < PARTICLE_MIN_PX) continue;
        if (sx < -16.0f || sx > SCREEN_W + 16.0f || sy < -16.0f || sy > SCREEN_H + 16.0f) continue;
        sp[i].x = sx; sp[i].y = sy; sp[i].ppm = ppm;
        bk = (int)(dz * ((float)DEPTH_BUCKETS / FAR_Z));
        bucket[i] = (uint8_t)(bk < 0 ? 0 : bk >= DEPTH_BUCKETS ? DEPTH_BUCKETS - 1 : bk);
        vis[i] = true;
        start[DEPTH_BUCKETS - 1 - bucket[i] + 1]++;   /* the farthest bucket sorts first */
        n++;
    }
    for (i = 1; i <= DEPTH_BUCKETS; i++) start[i] += start[i - 1];
    for (i = 0; i < PARTICLE_MAX; i++)
        if (vis[i]) order[start[DEPTH_BUCKETS - 1 - bucket[i]]++] = (int16_t)i;

    {   /* Leave room for what is drawn after the particles (impact stars, weapon). */
        int room = (int)(sizeof(rl->cmd) / sizeof(rl->cmd[0])) - PARTICLE_CMD_RESERVE - rl->n;
        int cap = room < PARTICLE_DRAW_MAX ? room : PARTICLE_DRAW_MAX;
        if (cap < 0) cap = 0;
        skip = n > cap ? n - cap : 0;   /* drop the farthest first */
    }
    for (i = skip; i < n; i++) {
        int k = order[i];
        const Particle* q = &pp->p[k];
        float px = size_m(q->kind) * sp[k].ppm;
        if (q->kind == PK_TOOTH) {
            float w = px < 2.0f ? 2.0f : px;
            rl_rect(rl, sp[k].x - w * 0.5f, sp[k].y - w, w, w * 1.3f, PAT_WHITE, true);
        } else {
            float s = px / PARTICLE_IMG_PX;
            int img = q->kind == PK_PUFF ? IMG_PUFF0 + q->variant % STUFF_PUFF_VARIANTS
                                         : IMG_SCRAP0 + q->variant % STUFF_SCRAP_VARIANTS;
            rl_bitmap(rl, img, sp[k].x, sp[k].y - px * 0.5f, q->kind == PK_SCRAP ? q->angle : 0.0f,
                      0.5f, 0.5f, s, s, false);
        }
    }
}
