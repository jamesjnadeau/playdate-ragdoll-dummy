#include "draw_world.h"
#include "pose.h"
#include "tuning.h"
#include "rmath.h"

static bool drawable(const Dummy* d) {
    return d->alive && d->state != DST_RAGDOLL && d->state != DST_DEAD;
}

void draw_world(RenderList* rl, const Camera* cam, const Hallway* hall,
                const DummyPool* dummies, const DummyRig rigs[DS_COUNT],
                const RagdollPool* ragdolls, int frame) {
    /* Dummies are items 0..MAX_DUMMIES-1, ragdolls MAX_DUMMIES.. ; sorted together, farthest first. */
    int order[MAX_DUMMIES + RAGDOLL_MAX];
    float key[MAX_DUMMIES + RAGDOLL_MAX];
    int n = 0, i, j;

    draw_hallway(rl, cam);
    for (i = hall->ndoors - 1; i >= 0; i--) draw_door(rl, cam, &hall->doors[i]);

    for (i = 0; i < MAX_DUMMIES + RAGDOLL_MAX; i++) {
        float z;
        if (i < MAX_DUMMIES) {
            if (!drawable(&dummies->d[i])) continue;
            z = dummies->d[i].z;
        } else {
            if (!ragdolls || !ragdolls->r[i - MAX_DUMMIES].active) continue;
            z = ragdolls->r[i - MAX_DUMMIES].p[RP_BODY_LO].z;
        }
        key[i] = z;
        for (j = n; j > 0 && key[order[j - 1]] < z; j--) order[j] = order[j - 1];
        order[j] = i;
        n++;
    }
    for (i = 0; i < n; i++) {
        if (order[i] >= MAX_DUMMIES) {
            const Ragdoll* r = &ragdolls->r[order[i] - MAX_DUMMIES];
            draw_ragdoll(rl, cam, r, &rigs[r->size]);
        } else {
            const Dummy* d = &dummies->d[order[i]];
            DummyPose pose;
            if (d->reaction_t > 0) pose_for_reaction((ReactionType)d->reaction, (float)d->reaction_t / (float)REACTION_FRAMES, d, frame, &pose);
            else pose_for_state(d, frame, &pose);
            draw_dummy(rl, cam, d, &rigs[d->size], &pose);
        }
    }
}

/* Start and end angle of the sweep for a swing. */
static void sweep_angles(const Swing* s, float* a0, float* a1) {
    if (s->side == SIDE_LEFT) { *a0 = -WEAPON_SIDE_DEG; *a1 = WEAPON_SIDE_DEG; }
    else if (s->side == SIDE_RIGHT) { *a0 = WEAPON_SIDE_DEG; *a1 = -WEAPON_SIDE_DEG; }
    else if (s->vert == VERT_UP) { *a0 = WEAPON_VERT_FAR_DEG; *a1 = WEAPON_VERT_NEAR_DEG; }
    else { *a0 = WEAPON_VERT_NEAR_DEG; *a1 = WEAPON_VERT_FAR_DEG; }
}

/* Point at distance r from the sweep pivot, at weapon angle deg (0 = straight up). */
static void polar(float deg, float r, float* x, float* y) {
    *x = WEAPON_PIVOT_X + r * rsin_deg(deg);
    *y = WEAPON_PIVOT_Y - r * rcos_deg(deg);
}

void draw_weapon(RenderList* rl, const Swing* s, const WeaponRig* wr, int image) {
    float cx = wr->w > 0 ? wr->grip.x / (float)wr->w : 0.5f;
    float cy = wr->h > 0 ? wr->grip.y / (float)wr->h : 0.9f;
    float a0, a1, a, gx, gy;
    int i;

    if (s->phase != SWS_SWEEP) {
        rl_bitmap(rl, image, WEAPON_REST_X, WEAPON_REST_Y, WEAPON_REST_DEG, cx, cy, 1.0f, 1.0f, false);
        return;
    }
    sweep_angles(s, &a0, &a1);
    a = lerpf(a0, a1, clampf(s->sweep01, 0.0f, 1.0f));
    polar(a, WEAPON_SWEEP_RADIUS, &gx, &gy);
    /* Speed lines trail behind the leading edge, along arcs at three distances up the weapon. */
    {
        float dir = a1 > a0 ? 1.0f : -1.0f;
        float len = wr->h > 0 ? (float)wr->h : 170.0f;
        for (i = 0; i < SPEED_LINES; i++) {
            float r = WEAPON_SWEEP_RADIUS + len * (0.45f + 0.25f * (float)i);
            float x0, y0, x1, y1;
            polar(a - dir * SPEED_LINE_GAP_DEG, r, &x0, &y0);
            polar(a - dir * (SPEED_LINE_GAP_DEG + SPEED_LINE_LEN_DEG), r, &x1, &y1);
            rl_line(rl, x0, y0, x1, y1, 2, PAT_BLACK);
        }
    }
    rl_bitmap(rl, image, gx, gy, a, cx, cy, 1.0f, 1.0f, false);
}

static void star(RenderList* rl, float x, float y, float r, Pattern p) {
    const float step = 360.0f / (float)IMPACT_POINTS;
    float ri = r * IMPACT_INNER_RATIO;
    int i;
    for (i = 0; i < IMPACT_POINTS; i++) {
        float t = (float)i * step;
        float xy[8];
        xy[0] = x; xy[1] = y;
        xy[2] = x + ri * rsin_deg(t - step * 0.5f); xy[3] = y - ri * rcos_deg(t - step * 0.5f);
        xy[4] = x + r * rsin_deg(t);                 xy[5] = y - r * rcos_deg(t);
        xy[6] = x + ri * rsin_deg(t + step * 0.5f); xy[7] = y - ri * rcos_deg(t + step * 0.5f);
        rl_poly(rl, xy, 4, p);
    }
}

void draw_impact(RenderList* rl, float x, float y, float r) {
    star(rl, x, y, r + IMPACT_OUTLINE_PX, PAT_BLACK);
    star(rl, x, y, r, PAT_WHITE);
}
