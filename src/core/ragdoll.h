#ifndef RAGDOLL_H
#define RAGDOLL_H

#include <stdbool.h>
#include "rmath.h"
#include "rig.h"
#include "dummy.h"
#include "hallway.h"
#include "camera.h"
#include "render_list.h"
#include "tuning.h"

enum { RP_BASE, RP_BODY_LO, RP_BODY_HI, RP_HEAD, RP_ELBOW_L, RP_FIST_L, RP_ELBOW_R, RP_FIST_R, RP_COUNT };
#define RAGDOLL_STICKS 9

typedef struct {
    bool active;
    DummySize size;
    Vec3 p[RP_COUNT], prev[RP_COUNT];    /* metres; velocity is (p - prev) / DT */
    float rest[RP_COUNT + 2];            /* stick rest lengths (m), RAGDOLL_STICKS used */
    float hp;
    float charged;                       /* wall damage already charged for the impact in the current window */
    int charge_t;                        /* frames until `charged` resets */
    int still_frames;
    int dummy;                           /* index of the owning Dummy, whose state follows the ragdoll */
    unsigned born;                       /* spawn serial, to recycle the oldest */
    uint8_t bowl_cd[MAX_DUMMIES];        /* frames until this ragdoll can bowl that dummy again */
} Ragdoll;
typedef struct { Ragdoll r[RAGDOLL_MAX]; unsigned serial; } RagdollPool;

/* Starts a ragdoll for dummies->d[dummy] from the rig's anchors with the given launch velocity (m/s).
   The oldest ragdoll is recycled when the pool is full (its dummy becomes DST_DEAD). Returns the index. */
int ragdoll_spawn(RagdollPool* p, DummyPool* dummies, int dummy, const DummyRig* rig, Vec3 launch);

typedef struct { int burst_count; int burst_idx[6]; Vec3 burst_at[6]; Vec3 burst_n[6]; int bowl_hits;
                     int knockouts;      /* dummies knocked out this step (door slams, bowling): already marked scored */
                     int door_slams; } RagdollStepOut;   /* open doors a ragdoll slammed shut */
/* One fixed step: Verlet, sticks, walls/floor/ceiling (impact damage, bursts), bowling into live dummies
   (chain knockouts spawn their own ragdolls), door slams (hall may be NULL) and settling. */
void ragdoll_step(RagdollPool* p, DummyPool* dummies, Hallway* hall, const DummyRig rigs[DS_COUNT],
                  RagdollStepOut* out);

/* Draws one part image per segment, rotated and stretched to the projected segment. */
void draw_ragdoll(RenderList* rl, const Camera* cam, const Ragdoll* r, const DummyRig* rig);

#endif
