#include "test.h"
#include "hallway.h"
#include "tuning.h"

static void test_door_counts(void) {
    Rng r;
    int i, seen[DS_COUNT] = {0, 0, 0};
    CHECK_EQ_INT(hallway_dummies_for_door(0.0f), 1);
    CHECK_EQ_INT(hallway_dummies_for_door(59.0f), 1);
    CHECK_EQ_INT(hallway_dummies_for_door(60.0f), 2);
    CHECK_EQ_INT(hallway_dummies_for_door(10000.0f), 5);
    rng_seed(&r, 7);
    for (i = 0; i < 200; i++) CHECK_EQ_INT(hallway_pick_size(&r, 10.0f), DS_SMALL);
    for (i = 0; i < 200; i++) seen[hallway_pick_size(&r, 100.0f)]++;
    CHECK(seen[DS_SMALL] > 0 && seen[DS_MEDIUM] > 0 && seen[DS_LARGE] > 0);
    for (i = 0; i < 200; i++) CHECK(hallway_pick_size(&r, 30.0f) != DS_LARGE);
}

static void test_doors_generated_ahead(void) {
    Rng r;
    Hallway h;
    int i;
    rng_seed(&r, 3);
    hallway_init(&h, &r);
    hallway_update(&h, &r, 0.0f);
    CHECK(h.ndoors >= 2);
    for (i = 0; i < h.ndoors; i++) CHECK(h.doors[i].z >= DOOR_FIRST_Z);
    for (i = 1; i < h.ndoors; i++) {
        float gap = h.doors[i].z - h.doors[i - 1].z - DOOR_LEN;
        CHECK(gap >= DOOR_GAP_MIN - 1e-4f && gap <= DOOR_GAP_MAX + 1e-4f);
    }
    CHECK(h.doors[h.ndoors - 1].z >= FAR_Z - DOOR_GAP_MAX);
}

static void test_door_opens_when_near(void) {
    Rng r;
    Hallway h;
    rng_seed(&r, 3);
    hallway_init(&h, &r);
    hallway_update(&h, &r, -3.0f);
    CHECK_NEAR(h.doors[0].z, 6.0f, 1e-4f);
    CHECK_EQ_INT(h.doors[0].state, DOOR_CLOSED_FULL);
    hallway_update(&h, &r, -1.9f);
    CHECK_EQ_INT(h.doors[0].state, DOOR_OPEN);
}

static void test_doors_recycle(void) {
    Rng r;
    Hallway h;
    int i;
    rng_seed(&r, 5);
    hallway_init(&h, &r);
    hallway_update(&h, &r, 0.0f);
    hallway_update(&h, &r, 40.0f);
    CHECK(h.recycled > 0);
    for (i = 0; i < h.ndoors; i++) CHECK(h.doors[i].z >= 40.0f - DOOR_RECYCLE_BEHIND);
    CHECK(h.ndoors <= MAX_DOORS);
}

void suite_hallway(void) {
    test_door_counts();
    test_doors_generated_ahead();
    test_door_opens_when_near();
    test_doors_recycle();
}
