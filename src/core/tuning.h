#ifndef TUNING_H
#define TUNING_H

#define DT (1.0f/30.0f)
#define HALL_HALF_W 1.6f
#define HALL_H 2.7f
#define CAM_Y 1.35f
#define SCREEN_W 400.0f
#define SCREEN_H 240.0f
#define FOCAL 150.0f
#define SCREEN_CX 200.0f
#define HORIZON_Y 108.0f
#define AIM_PITCH_LOW_PX (-40.0f)   /* horizon moves up when looking down */
#define AIM_PITCH_HIGH_PX 40.0f
#define AIM_EASE_PX_PER_FRAME 8.0f
#define NEAR_Z 0.25f
#define NEAR_Z_EPS 0.0005f           /* float rounding slack on the near-plane test */
#define FAR_Z 16.0f
#define PLAYER_SPEED 1.6f
#define PLAYER_RADIUS 0.35f
/* Owner wants this configurable: how far (m) the player may walk back from the nearest open door. */
#define WALK_BACK_LIMIT 3.0f
#define PLAYER_HEARTS 5
#define PLAYER_INVULN_FRAMES 20
#define KNOCKBACK_DIST 1.0f
#define KNOCKBACK_FRAMES 6
#define DUMMY_H_SMALL 1.0f
#define DUMMY_H_MEDIUM 1.45f
#define DUMMY_H_LARGE 1.85f
#define DUMMY_SPEED_SMALL 0.9f
#define DUMMY_SPEED_MEDIUM 0.75f
#define DUMMY_SPEED_LARGE 0.55f
#define DUMMY_RADIUS_SMALL 0.30f
#define DUMMY_RADIUS_MEDIUM 0.38f
#define DUMMY_RADIUS_LARGE 0.50f
#define DUMMY_HP_SMALL 2.0f
#define DUMMY_HP_MEDIUM 3.0f
#define DUMMY_HP_LARGE 5.0f
#define DUMMY_WINDUP_SMALL 12
#define DUMMY_WINDUP_MEDIUM 16
#define DUMMY_WINDUP_LARGE 24
#define DUMMY_DMG_SMALL 1
#define DUMMY_DMG_MEDIUM 1
#define DUMMY_DMG_LARGE 2
#define DUMMY_PUNCH_REACH 0.75f
#define DUMMY_STAGGER_FRAMES 10
#define DUMMY_HIT_PUSHBACK 0.35f
#define DUMMY_EMERGE_FRAMES 15
#define REACTION_FRAMES 12
#define SWING_TRIGGER_DEG 12.0f      /* crank degrees in one frame to start */
#define SWING_WINDOW 4               /* frames to measure peak speed */
#define SWING_SPEED_REF 30.0f        /* deg/frame that gives factor 1.0 */
#define SWING_FACTOR_MIN 0.5f
#define SWING_FACTOR_MAX 1.5f
#define SWING_SWEEP_FRAMES 6
#define SWING_RECOVER_FRAMES 8
#define HEAD_MULT 1.5f
#define GRAVITY 9.8f
#define RAGDOLL_ITER 4
#define RAGDOLL_RESTITUTION 0.45f
#define RAGDOLL_FRICTION 0.8f
#define RAGDOLL_HP 6.0f
#define RAGDOLL_IMPACT_MIN 3.0f      /* m/s; below this no ragdoll damage */
#define RAGDOLL_BOWL_MIN 2.0f        /* m/s to damage a live dummy */
#define RAGDOLL_BOWL_DMG 0.4f        /* hp per m/s */
#define RAGDOLL_DOORSLAM_DMG 1.0f
#define RAGDOLL_REST_FRAMES 90       /* settled ragdoll fades after this */
#define RAGDOLL_MAX 6
#define RAGDOLL_SETTLE_SPEED 0.2f    /* m/s; every point slower than this counts as still */
#define RAGDOLL_BOUNCE_MIN 0.7f      /* m/s; slower contacts do not bounce, so a ragdoll can lie still */
#define RAGDOLL_HEAD_VEL 1.1f        /* launch velocity multipliers, for floppiness */
#define RAGDOLL_FIST_VEL 1.2f
#define RAGDOLL_LEAN 0.05f           /* spawn pose leans this much (m per m of height) so it topples */
#define RAGDOLL_IMPACT_WINDOW 6      /* frames in which one impact is charged only once (max, not sum) */
#define RAGDOLL_LAUNCH_SPIN 0.35f    /* head/base opposite horizontal velocity, fraction of launch speed */
#define RAGDOLL_BOWL_PUSH 0.3f       /* m a bowled dummy is shoved along the ragdoll's velocity */
#define RAGDOLL_BOWL_COOLDOWN 10     /* frames before the same ragdoll-dummy pair can hit again */
#define RAGDOLL_BOWL_LAUNCH 0.6f     /* a bowled-out dummy flies at this fraction of the hitting speed */
#define RAGDOLL_BASE_HIDE_Y 0.05f    /* the black base ellipse is not drawn above this height (m) */
#define PARTICLE_LIFE_FRAMES 150
#define BURST_PUFFS 26
#define BURST_SCRAPS 9
#define DOOR_LEN 1.1f
#define DOOR_FIRST_Z 6.0f
#define DOOR_GAP_MIN 3.0f
#define DOOR_GAP_MAX 6.0f
#define DOOR_ACTIVATE_DIST 8.0f
#define DOOR_SPAWN_INTERVAL 45
#define DOOR_RECYCLE_BEHIND 4.0f
/* Open angle (deg from closed) of a fully open door; owner wants it configurable. Near 180 lies along the wall. */
#define DOOR_OPEN_MAX_DEG 165.0f
#define DOOR_LEAF_STEP 0.1f
#define DOOR_MAX_DUMMIES 5
#define DOOR_DUMMIES_PER_Z 60.0f
#define DOOR_MEDIUM_FROM_Z 20.0f
#define DOOR_LARGE_FROM_Z 60.0f
#define DUMMY_PUNCH_FRAMES 4
#define DUMMY_PUNCH_COOLDOWN 20
#define DUMMY_SEPARATION_ITERS 2
#define DUMMY_TARGET_Z_OFFSET 0.1f
#define DOOR_SPAWN_INSET 0.3f
#define WEAPON_OFFER_EVERY_DOORS 4
/* Weapon drawing (screen px, degrees; 0 deg points the weapon straight up). */
#define WEAPON_REST_X 352.0f
#define WEAPON_REST_Y 262.0f
#define WEAPON_REST_DEG -53.0f
#define WEAPON_PIVOT_X 200.0f
#define WEAPON_PIVOT_Y 300.0f
#define WEAPON_SWEEP_RADIUS 150.0f
#define WEAPON_SIDE_DEG 70.0f
#define WEAPON_VERT_NEAR_DEG 20.0f
#define WEAPON_VERT_FAR_DEG 160.0f
#define SPEED_LINES 3
#define SPEED_LINE_GAP_DEG 6.0f
#define SPEED_LINE_LEN_DEG 22.0f
/* Impact stars and hurt feedback. */
#define IMPACT_FRAMES 4
#define MAX_IMPACTS 8
#define IMPACT_POINTS 14
#define IMPACT_OUTLINE_PX 2.0f
#define IMPACT_INNER_RATIO 0.5f
#define IMPACT_MIN_R 8.0f
#define IMPACT_MAX_R 30.0f
#define IMPACT_R_PER_PPM 0.22f       /* star radius per pixel-per-metre of projection */
#define HURT_SHAKE_FRAMES 6
#define HURT_SHAKE_PX 3

#endif
