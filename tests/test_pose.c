#include <string.h>
#include "test.h"
#include "pose.h"
#include "tuning.h"

static void same(const DummyPose* a, const DummyPose* b) {
    CHECK_NEAR(a->lean_deg, b->lean_deg, 0.0001f);
    CHECK_NEAR(a->sx, b->sx, 0.0001f);
    CHECK_NEAR(a->sy, b->sy, 0.0001f);
    CHECK_NEAR(a->head_dy, b->head_dy, 0.0001f);
    CHECK_NEAR(a->head_rot_deg, b->head_rot_deg, 0.0001f);
    CHECK_NEAR(a->head_sy, b->head_sy, 0.0001f);
    CHECK_NEAR(a->arm_l_deg, b->arm_l_deg, 0.0001f);
    CHECK_NEAR(a->arm_r_deg, b->arm_r_deg, 0.0001f);
    CHECK_EQ_INT(a->face, b->face);
    CHECK_EQ_INT(a->teeth, b->teeth);
    CHECK_EQ_INT(a->stars, b->stars);
}

static void test_pose_upward_head_back(void) {
    Dummy d; DummyPose p, n, w;
    memset(&d, 0, sizeof d);
    d.alive = true; d.size = DS_MEDIUM; d.state = DST_STAGGER;
    pose_for_reaction(RX_UPWARD, 1.0f, &d, 0, &p);
    CHECK_NEAR(p.head_dy, -18.0f, 0.0001f);
    CHECK_NEAR(p.head_rot_deg, -17.0f, 0.0001f);
    CHECK_NEAR(p.head_sy, 0.82f, 0.0001f);
    CHECK_NEAR(p.lean_deg, -6.0f, 0.0001f);
    CHECK_NEAR(p.arm_l_deg, 130.0f, 0.0001f);
    CHECK_EQ_INT(p.face, PART_FACE_WHACK);
    CHECK(p.teeth);
    pose_for_reaction(RX_UPWARD, 0.0f, &d, 0, &p);
    d.state = DST_WALK;
    pose_for_state(&d, 0, &n);
    same(&p, &n);
    /* mid-reaction: strictly between neutral and peak */
    pose_for_reaction(RX_UPWARD, 0.5f, &d, 0, &w);
    CHECK(w.head_dy < 0.0f && w.head_dy > -18.0f);
    CHECK(!w.teeth);
}

static void test_pose_reaction_end_matches_walk_frame(void) {
    Dummy d; DummyPose p, n;
    int frames[3] = { 0, 7, 19 };
    memset(&d, 0, sizeof d);
    d.alive = true; d.size = DS_MEDIUM; d.state = DST_STAGGER;
    for (int i = 0; i < 3; i++) {
        pose_for_reaction(RX_UPWARD, 0.0f, &d, frames[i], &p);
        d.state = DST_WALK;
        pose_for_state(&d, frames[i], &n);
        d.state = DST_STAGGER;
        same(&p, &n);      /* no pop when the reaction ends on any frame */
    }
}

static void test_pose_from_left_mirror(void) {
    Dummy d; DummyPose l, r;
    memset(&d, 0, sizeof d);
    d.alive = true; d.size = DS_SMALL; d.state = DST_STAGGER;
    pose_for_reaction(RX_FROM_LEFT, 1.0f, &d, 0, &l);
    pose_for_reaction(RX_FROM_RIGHT, 1.0f, &d, 0, &r);
    CHECK_NEAR(l.lean_deg, 20.0f, 0.0001f);
    CHECK_NEAR(l.lean_deg, -r.lean_deg, 0.0001f);
    CHECK_NEAR(l.arm_l_deg, 145.0f, 0.0001f);
    CHECK_NEAR(l.arm_r_deg, -30.0f, 0.0001f);
    CHECK_NEAR(r.arm_r_deg, 145.0f, 0.0001f);
    CHECK_EQ_INT(l.face, PART_FACE_HURT);
}

static void test_pose_downward_and_diagonal(void) {
    Dummy d; DummyPose p;
    memset(&d, 0, sizeof d);
    d.alive = true; d.size = DS_LARGE; d.state = DST_STAGGER;
    pose_for_reaction(RX_DOWNWARD, 1.0f, &d, 0, &p);
    CHECK_NEAR(p.sx, 1.28f, 0.0001f);
    CHECK_NEAR(p.sy, 0.72f, 0.0001f);
    CHECK_NEAR(p.arm_l_deg, 75.0f, 0.0001f);
    CHECK_NEAR(p.elbow_l_deg, -35.0f, 0.0001f);
    CHECK_EQ_INT(p.face, PART_FACE_SQUISH);
    CHECK(p.stars);
    pose_for_reaction(RX_LEFT_UP, 1.0f, &d, 0, &p);
    CHECK_NEAR(p.lean_deg, 20.0f, 0.0001f);
    CHECK_NEAR(p.head_dy, -18.0f, 0.0001f);
    pose_for_reaction(RX_NONE, 1.0f, &d, 0, &p);
    CHECK(!p.teeth && !p.stars);
}

static DummyRig g_rig[DS_COUNT];

static void setup_box(DummySize sz, float z, Dummy* d, DummyPose* p) {
    for (int i = 0; i < DS_COUNT; i++) rig_default((DummySize)i, &g_rig[i]);
    memset(d, 0, sizeof *d);
    d->alive = true; d->size = sz; d->state = DST_WALK; d->x = 0.0f; d->z = z;
    pose_for_state(d, 0, p);
}

static Camera box_cam(void) {
    Camera cam;
    memset(&cam, 0, sizeof cam);
    camera_update(&cam);
    return cam;
}

static void test_hit_boxes_contain_anchor_points(void) {
    Camera cam = box_cam();
    Dummy d; DummyPose p; ScreenBox hd, bd;
    const DummyRig* rig;
    Vec3 w; float sx, sy, ppm;
    setup_box(DS_MEDIUM, 2.0f, &d, &p);
    rig = &g_rig[DS_MEDIUM];
    CHECK(dummy_hit_boxes(&cam, &d, rig, &p, &hd, &bd));
    w.x = d.x; w.z = d.z;
    w.y = -rig->face_center.y / rig->height_px * dummy_height(d.size);
    CHECK(camera_project(&cam, w, &sx, &sy, &ppm));
    CHECK(sx >= hd.x0 && sx <= hd.x1 && sy >= hd.y0 && sy <= hd.y1);
    w.y = -rig->body_center.y / rig->height_px * dummy_height(d.size);
    CHECK(camera_project(&cam, w, &sx, &sy, &ppm));
    CHECK(sx >= bd.x0 && sx <= bd.x1 && sy >= bd.y0 && sy <= bd.y1);
}

static void test_head_above_body(void) {
    Camera cam = box_cam();
    Dummy d; DummyPose p; ScreenBox hd, bd;
    float sx, sy, ppm; Vec3 w;
    setup_box(DS_MEDIUM, 2.0f, &d, &p);
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &hd, &bd));
    CHECK(hd.y1 <= bd.y0 + 1.0f);
    CHECK(hd.x0 <= hd.x1 && hd.y0 <= hd.y1 && bd.x0 <= bd.x1 && bd.y0 <= bd.y1);
    w.x = d.x; w.y = p.lift_m; w.z = d.z;
    CHECK(camera_project(&cam, w, &sx, &sy, &ppm));
    CHECK_NEAR((hd.x0 + hd.x1) * 0.5f, sx, 2.0f);
    CHECK_NEAR((bd.x0 + bd.x1) * 0.5f, sx, 2.0f);
}

static void test_boxes_scale_with_size(void) {
    Camera cam = box_cam();
    Dummy d; DummyPose p; ScreenBox hs, bs, hl, bl, hf, bf;
    setup_box(DS_SMALL, 2.0f, &d, &p);
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_SMALL], &p, &hs, &bs));
    setup_box(DS_LARGE, 2.0f, &d, &p);
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_LARGE], &p, &hl, &bl));
    CHECK((bl.y1 - bl.y0) > (bs.y1 - bs.y0));
    setup_box(DS_MEDIUM, 2.0f, &d, &p);
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &hs, &bs));
    d.z = 6.0f;
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &hf, &bf));
    CHECK((bf.y1 - bf.y0) < (bs.y1 - bs.y0));
    CHECK((bf.x1 - bf.x0) < (bs.x1 - bs.x0));
}

static void test_boxes_none_when_not_drawn(void) {
    Camera cam = box_cam();
    Dummy d; DummyPose p; ScreenBox hd, bd;
    setup_box(DS_MEDIUM, 2.0f, &d, &p);
    d.z = cam.player_z + 0.1f;
    CHECK(!dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &hd, &bd));
    d.z = cam.player_z + FAR_Z + 1.0f;
    CHECK(!dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &hd, &bd));
}

static void test_boxes_follow_pitch(void) {
    Camera cam = box_cam();
    Dummy d; DummyPose p; ScreenBox h1, b1, h2, b2;
    setup_box(DS_MEDIUM, 2.0f, &d, &p);
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &h1, &b1));
    cam.pitch_px = 40.0f;
    CHECK(dummy_hit_boxes(&cam, &d, &g_rig[DS_MEDIUM], &p, &h2, &b2));
    CHECK(h2.y0 != h1.y0);
    CHECK(b2.y0 != b1.y0);
}

void suite_pose(void) {
    test_hit_boxes_contain_anchor_points();
    test_head_above_body();
    test_boxes_scale_with_size();
    test_boxes_none_when_not_drawn();
    test_boxes_follow_pitch();
    test_pose_reaction_end_matches_walk_frame();
    test_pose_upward_head_back();
    test_pose_from_left_mirror();
    test_pose_downward_and_diagonal();
}
