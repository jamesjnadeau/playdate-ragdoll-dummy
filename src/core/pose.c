#include <math.h>
#include "pose.h"
#include "tuning.h"

#define WADDLE_DEG 4.0f
#define WADDLE_STEP_DEG 24.0f      /* 2 Hz at 30 fps: 360 * 2 / 30 */
#define ARM_REST_DEG 8.0f
#define ARM_SWING_DEG 12.0f
#define WINDUP_LEAN_DEG (-8.0f)
#define WINDUP_ARM_DEG 150.0f
#define PUNCH_LEAN_DEG 6.0f
#define PUNCH_SCALE 1.08f
#define STAGGER_LEAN_DEG 10.0f
#define EMERGE_MIN_SX 0.6f
#define BOUNCE_M 0.02f

void pose_for_state(const Dummy* d, int frame, DummyPose* o) {
    float w = rsin_deg(WADDLE_STEP_DEG * (float)frame);
    o->lean_deg = 0.0f; o->sx = 1.0f; o->sy = 1.0f; o->lift_m = 0.0f;
    o->head_dx = 0.0f; o->head_dy = 0.0f; o->head_rot_deg = 0.0f; o->head_sy = 1.0f;
    o->arm_l_deg = ARM_REST_DEG; o->arm_r_deg = ARM_REST_DEG;
    o->elbow_l_deg = 0.0f; o->elbow_r_deg = 0.0f;
    o->face = PART_FACE_MEAN;

    switch (d->state) {
    case DST_EMERGING: {
        float p = 1.0f - (float)d->timer / (float)DUMMY_EMERGE_FRAMES;
        o->sx = lerpf(EMERGE_MIN_SX, 1.0f, clampf(p, 0.0f, 1.0f));
        o->lean_deg = WADDLE_DEG * w;
        break;
    }
    case DST_WALK:
        o->lean_deg = WADDLE_DEG * w;
        o->lift_m = BOUNCE_M * fabsf(w);
        o->arm_l_deg = ARM_REST_DEG + ARM_SWING_DEG * w;
        o->arm_r_deg = ARM_REST_DEG - ARM_SWING_DEG * w;
        break;
    case DST_WINDUP: {
        float p = 1.0f - (float)d->timer / (float)dummy_windup_frames(d->size);
        p = clampf(p, 0.0f, 1.0f);
        o->lean_deg = WINDUP_LEAN_DEG * p;
        o->arm_r_deg = lerpf(ARM_REST_DEG, WINDUP_ARM_DEG, p);
        o->elbow_r_deg = 20.0f * p;
        o->arm_l_deg = lerpf(ARM_REST_DEG, 40.0f, p);
        break;
    }
    case DST_PUNCH:
        o->lean_deg = PUNCH_LEAN_DEG;
        o->sx = PUNCH_SCALE; o->sy = PUNCH_SCALE;
        o->arm_r_deg = 20.0f;
        o->arm_l_deg = 40.0f;
        break;
    case DST_STAGGER:
        o->lean_deg = STAGGER_LEAN_DEG;
        o->head_rot_deg = 12.0f;
        o->arm_l_deg = 50.0f; o->arm_r_deg = 50.0f;
        o->face = PART_FACE_HURT;
        break;
    case DST_RAGDOLL:
    case DST_DEAD:
        break;
    }
}

/* Rotates v clockwise on screen (y down) by deg. */
static Vec2 rot(Vec2 v, float deg) {
    float s = rsin_deg(deg), c = rcos_deg(deg);
    Vec2 r;
    r.x = v.x * c - v.y * s;
    r.y = v.x * s + v.y * c;
    return r;
}

typedef struct {
    RenderList* rl;
    float bx, by, s;        /* base screen position, px per rig px */
    float lean, lsin, lcos, psx, psy;   /* lean degrees (with cached sin/cos) and pose scale */
    int size;
} DrawCtx;

/* Rig-space offset from the base -> screen, applying pose scale and lean. */
static Vec2 to_screen(const DrawCtx* c, Vec2 a) {
    Vec2 v;
    v.x = a.x * c->psx; v.y = a.y * c->psy;
    { float x = v.x * c->lcos - v.y * c->lsin; v.y = v.x * c->lsin + v.y * c->lcos; v.x = x; }
    v.x = c->bx + v.x * c->s;
    v.y = c->by + v.y * c->s;
    return v;
}

static void part(const DrawCtx* c, DummyPart p, Vec2 at, float ang, float cx, float cy, float ysq) {
    Vec2 sp = to_screen(c, at);
    rl_bitmap(c->rl, img_dummy((DummySize)c->size, p), sp.x, sp.y, c->lean + ang,
              cx, cy, c->s * c->psx, c->s * c->psy * ysq, false);
}

/* Arm direction for a hanging arm turned outward by deg (left arm: toward -x). */
static Vec2 arm_dir(float deg, float side) {
    Vec2 v = { 0.0f, 1.0f };
    return rot(v, -side * deg);
}

static void draw_arm(const DrawCtx* c, const DummyRig* rig, Vec2 shoulder, float side,
                     float arm_deg, float elbow_deg) {
    float a1 = -side * arm_deg, a2 = -side * (arm_deg + elbow_deg);
    Vec2 d1 = arm_dir(arm_deg, side), d2 = arm_dir(arm_deg + elbow_deg, side);
    Vec2 elbow = { shoulder.x + d1.x * rig->upper_arm_px, shoulder.y + d1.y * rig->upper_arm_px };
    Vec2 hand = { elbow.x + d2.x * rig->lower_arm_px, elbow.y + d2.y * rig->lower_arm_px };
    part(c, PART_ARM_UPPER, shoulder, a1, 0.5f, 0.0f, 1.0f);
    part(c, PART_ARM_LOWER, elbow, a2, 0.5f, 0.0f, 1.0f);
    part(c, PART_FIST, hand, a2, 0.5f, 0.5f, 1.0f);
}

void draw_dummy(RenderList* rl, const Camera* cam, const Dummy* d, const DummyRig* rig, const DummyPose* pose) {
    Vec3 w;
    float sx, sy, ppm;
    DrawCtx c;
    Vec2 origin = { 0.0f, 0.0f }, head, face;

    w.x = d->x; w.y = pose->lift_m; w.z = d->z;
    if (!camera_project(cam, w, &sx, &sy, &ppm)) return;
    if (d->z - cam->player_z > FAR_Z) return;
    if (sx + ppm * dummy_height(d->size) < 0.0f || sx - ppm * dummy_height(d->size) > 400.0f) return;
    c.rl = rl; c.bx = sx; c.by = sy;
    c.s = ppm * dummy_height(d->size) / rig->height_px;
    c.lean = pose->lean_deg; c.lsin = rsin_deg(c.lean); c.lcos = rcos_deg(c.lean); c.psx = pose->sx; c.psy = pose->sy;
    c.size = (int)d->size;

    part(&c, PART_BASE, origin, 0.0f, 0.5f, 0.5f, 1.0f);
    part(&c, PART_BODY, rig->body_center, 0.0f, 0.5f, 0.5f, 1.0f);
    draw_arm(&c, rig, rig->shoulder_l, -1.0f, pose->arm_l_deg, pose->elbow_l_deg);
    draw_arm(&c, rig, rig->shoulder_r, 1.0f, pose->arm_r_deg, pose->elbow_r_deg);
    head.x = rig->neck.x + pose->head_dx; head.y = rig->neck.y + pose->head_dy;
    face.x = rig->face_center.x + pose->head_dx; face.y = rig->face_center.y + pose->head_dy;
    /* The head image sits on the neck anchor (bottom centre); the face is centred on its anchor. */
    part(&c, PART_HEAD, head, pose->head_rot_deg, 0.5f, 1.0f, pose->head_sy);
    part(&c, pose->face, face, pose->head_rot_deg, 0.5f, 0.5f, pose->head_sy);
}

/* ---- doors ---- */

#define DOOR_H 2.1f
#define FRAME_T 0.09f           /* frame thickness in metres */
#define PANEL_DETAIL_DIST 9.0f

static void dproj(const Camera* cam, float x, float y, float z, float* sx, float* sy) {
    Vec3 w;
    float ppm;
    w.x = x; w.y = y; w.z = z;
    if (z < cam->player_z + NEAR_Z) w.z = cam->player_z + NEAR_Z;
    camera_project(cam, w, sx, sy, &ppm);
}

/* Quad in the wall plane spanning z0..z1 and y0..y1. */
static void wall_quad(RenderList* rl, const Camera* cam, float x, float z0, float z1,
                      float y0, float y1, Pattern p) {
    float xy[8];
    dproj(cam, x, y0, z0, &xy[0], &xy[1]);
    dproj(cam, x, y1, z0, &xy[2], &xy[3]);
    dproj(cam, x, y1, z1, &xy[4], &xy[5]);
    dproj(cam, x, y0, z1, &xy[6], &xy[7]);
    rl_poly(rl, xy, 4, p);
}

static void wall_line(RenderList* rl, const Camera* cam, float x, float za, float ya,
                      float zb, float yb, int width) {
    float ax, ay, bx, by;
    dproj(cam, x, ya, za, &ax, &ay);
    dproj(cam, x, yb, zb, &bx, &by);
    rl_line(rl, ax, ay, bx, by, width, PAT_BLACK);
}

static void wall_box(RenderList* rl, const Camera* cam, float x, float z0, float z1, float y0, float y1) {
    wall_line(rl, cam, x, z0, y0, z1, y0, 1);
    wall_line(rl, cam, x, z1, y0, z1, y1, 1);
    wall_line(rl, cam, x, z1, y1, z0, y1, 1);
    wall_line(rl, cam, x, z0, y1, z0, y0, 1);
}

void door_leaf_free_edge(const Door* door, float* x, float* z) {
    float ang = clampf(door->leaf_angle, 0.0f, 1.0f) * DOOR_OPEN_MAX_DEG;
    /* Hinged on the wall at the near edge (door->z); swings into the hallway (-side). At 90 deg it is
       perpendicular to the wall; at DOOR_OPEN_MAX_DEG it lies nearly along the wall on the player's side. */
    *x = (float)door->side * (HALL_HALF_W - DOOR_LEN * rsin_deg(ang));
    *z = door->z + DOOR_LEN * rcos_deg(ang);
}

void draw_door(RenderList* rl, const Camera* cam, const Door* door) {
    float x = (float)door->side * HALL_HALF_W;
    float z0 = door->z, z1 = door->z + DOOR_LEN;
    float dz = z0 - cam->player_z;
    if (z1 < cam->player_z + NEAR_Z) return;

    if (door->state == DOOR_OPEN) {
        float xf, zf;
        float xy[8];
        door_leaf_free_edge(door, &xf, &zf);
        wall_quad(rl, cam, x, z0, z1, 0.0f, DOOR_H, PAT_BLACK);   /* doorway */
        /* Hinged at the near edge. */
        dproj(cam, x, 0.0f, z0, &xy[0], &xy[1]);
        dproj(cam, x, DOOR_H, z0, &xy[2], &xy[3]);
        dproj(cam, xf, DOOR_H, zf, &xy[4], &xy[5]);
        dproj(cam, xf, 0.0f, zf, &xy[6], &xy[7]);
        rl_poly(rl, xy, 4, PAT_WHITE);
        rl_line(rl, xy[0], xy[1], xy[2], xy[3], 2, PAT_BLACK);
        rl_line(rl, xy[2], xy[3], xy[4], xy[5], 2, PAT_BLACK);
        rl_line(rl, xy[4], xy[5], xy[6], xy[7], 2, PAT_BLACK);
        rl_line(rl, xy[6], xy[7], xy[0], xy[1], 2, PAT_BLACK);
        return;
    }

    /* Closed: black frame, white leaf, two panels and a knob. */
    {
        float iz0 = z0 + FRAME_T, iz1 = z1 - FRAME_T, iy1 = DOOR_H - FRAME_T;
        float zm = (z0 + z1) * 0.5f;
        wall_quad(rl, cam, x, z0, z1, 0.0f, DOOR_H, PAT_BLACK);
        wall_quad(rl, cam, x, iz0, iz1, 0.0f, iy1, PAT_WHITE);
        if (dz < PANEL_DETAIL_DIST) {
            float pz = 0.14f;
            wall_box(rl, cam, x, iz0 + pz, zm - 0.05f, 1.15f, iy1 - 0.15f);
            wall_box(rl, cam, x, zm + 0.05f, iz1 - pz, 1.15f, iy1 - 0.15f);
            wall_box(rl, cam, x, iz0 + pz, zm - 0.05f, 0.2f, 0.95f);
            wall_box(rl, cam, x, zm + 0.05f, iz1 - pz, 0.2f, 0.95f);
        }
        {
            float kz = door->side > 0 ? iz0 + 0.12f : iz1 - 0.12f;
            float kx, ky;
            dproj(cam, x, 1.0f, kz, &kx, &ky);
            rl_rect(rl, kx - 1.0f, ky - 1.0f, 3.0f, 3.0f, PAT_BLACK, false);
        }
    }
}
