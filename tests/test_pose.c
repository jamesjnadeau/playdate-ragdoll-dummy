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
    pose_for_reaction(RX_UPWARD, 1.0f, &d, &p);
    CHECK_NEAR(p.head_dy, -18.0f, 0.0001f);
    CHECK_NEAR(p.head_rot_deg, -17.0f, 0.0001f);
    CHECK_NEAR(p.head_sy, 0.82f, 0.0001f);
    CHECK_NEAR(p.lean_deg, -6.0f, 0.0001f);
    CHECK_NEAR(p.arm_l_deg, 130.0f, 0.0001f);
    CHECK_EQ_INT(p.face, PART_FACE_WHACK);
    CHECK(p.teeth);
    pose_for_reaction(RX_UPWARD, 0.0f, &d, &p);
    d.state = DST_WALK;
    pose_for_state(&d, 0, &n);
    same(&p, &n);
    /* mid-reaction: strictly between neutral and peak */
    pose_for_reaction(RX_UPWARD, 0.5f, &d, &w);
    CHECK(w.head_dy < 0.0f && w.head_dy > -18.0f);
    CHECK(!w.teeth);
}

static void test_pose_from_left_mirror(void) {
    Dummy d; DummyPose l, r;
    memset(&d, 0, sizeof d);
    d.alive = true; d.size = DS_SMALL; d.state = DST_STAGGER;
    pose_for_reaction(RX_FROM_LEFT, 1.0f, &d, &l);
    pose_for_reaction(RX_FROM_RIGHT, 1.0f, &d, &r);
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
    pose_for_reaction(RX_DOWNWARD, 1.0f, &d, &p);
    CHECK_NEAR(p.sx, 1.28f, 0.0001f);
    CHECK_NEAR(p.sy, 0.72f, 0.0001f);
    CHECK_NEAR(p.arm_l_deg, 75.0f, 0.0001f);
    CHECK_NEAR(p.elbow_l_deg, -35.0f, 0.0001f);
    CHECK_EQ_INT(p.face, PART_FACE_SQUISH);
    CHECK(p.stars);
    pose_for_reaction(RX_LEFT_UP, 1.0f, &d, &p);
    CHECK_NEAR(p.lean_deg, 20.0f, 0.0001f);
    CHECK_NEAR(p.head_dy, -18.0f, 0.0001f);
    pose_for_reaction(RX_NONE, 1.0f, &d, &p);
    CHECK(!p.teeth && !p.stars);
}

void suite_pose(void) {
    test_pose_upward_head_back();
    test_pose_from_left_mirror();
    test_pose_downward_and_diagonal();
}
