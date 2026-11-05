#include "draw_world.h"
#include "pose.h"

static bool drawable(const Dummy* d) {
    return d->alive && d->state != DST_RAGDOLL && d->state != DST_DEAD;
}

void draw_world(RenderList* rl, const Camera* cam, const Hallway* hall,
                const DummyPool* dummies, const DummyRig rigs[DS_COUNT], int frame) {
    int order[MAX_DUMMIES];
    int n = 0, i, j;

    draw_hallway(rl, cam);
    for (i = hall->ndoors - 1; i >= 0; i--) draw_door(rl, cam, &hall->doors[i]);

    for (i = 0; i < MAX_DUMMIES; i++) {
        if (!drawable(&dummies->d[i])) continue;
        /* insertion sort, farthest first */
        for (j = n; j > 0 && dummies->d[order[j - 1]].z < dummies->d[i].z; j--) order[j] = order[j - 1];
        order[j] = i;
        n++;
    }
    for (i = 0; i < n; i++) {
        const Dummy* d = &dummies->d[order[i]];
        DummyPose pose;
        pose_for_state(d, frame, &pose);
        draw_dummy(rl, cam, d, &rigs[d->size], &pose);
    }
}
