#ifndef TUNING_H
#define TUNING_H

#define DT (1.0f/30.0f)
#define HALL_HALF_W 1.6f
#define HALL_H 2.7f
#define CAM_Y 1.35f
#define FOCAL 150.0f
#define SCREEN_CX 200.0f
#define HORIZON_Y 108.0f
#define AIM_PITCH_LOW_PX (-40.0f)   /* horizon moves up when looking down */
#define AIM_PITCH_HIGH_PX 40.0f
#define AIM_EASE_PX_PER_FRAME 8.0f
#define NEAR_Z 0.25f
#define FAR_Z 16.0f
#define PLAYER_SPEED 1.6f
#define PLAYER_RADIUS 0.35f
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
#define PARTICLE_LIFE_FRAMES 150
#define BURST_PUFFS 26
#define BURST_SCRAPS 9
#define DOOR_LEN 1.1f
#define DOOR_FIRST_Z 6.0f
#define DOOR_GAP_MIN 3.0f
#define DOOR_GAP_MAX 6.0f
#define DOOR_ACTIVATE_DIST 8.0f
#define DOOR_SPAWN_INTERVAL 45
#define WEAPON_OFFER_EVERY_DOORS 4

#endif
