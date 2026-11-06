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
#define HORIZON_Y 72.0f
#define CAM_PITCH_PX (-20.0f)       /* fixed view: halfway between looking down (-40) and level (0) */
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
/* Owner wants this configurable: dummy health grows by this fraction of its base per 100 m of spawn distance. */
#define DUMMY_HP_PER_100M 0.25f
/* Owner wants this configurable: doors hold their next dummy while this many are alive (ragdolls and dead do not count). */
#define MAX_LIVE_DUMMIES 10
#define DUMMY_WINDUP_SMALL 12
#define DUMMY_WINDUP_MEDIUM 16
#define DUMMY_WINDUP_LARGE 24
#define DUMMY_DMG_SMALL 1
#define DUMMY_DMG_MEDIUM 1
#define DUMMY_DMG_LARGE 2
#define DUMMY_PUNCH_REACH_FRAC 0.5f  /* dummies punch once halfway into the weapon's reach */
#define DUMMY_PUNCH_GAP 0.1f          /* ...or from this close to the player, if that is further (big dummies) */
#define DUMMY_STAGGER_FRAMES 10
#define DUMMY_HIT_PUSHBACK 0.35f
#define DUMMY_EMERGE_FRAMES 15
#define REACTION_FRAMES 12
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
#define BURST_FLOOR_PUFFS 12         /* extra puffs thrown at the floor below a burst, to form the pile */
#define PARTICLE_MAX 256
#define PARTICLE_PUFF_SPEED_MIN 1.0f /* burst spray speeds (m/s) */
#define PARTICLE_PUFF_SPEED_MAX 4.0f
#define PARTICLE_SCRAP_SPEED_MIN 3.0f
#define PARTICLE_SCRAP_SPEED_MAX 7.0f
#define PARTICLE_FLOOR_SPREAD 1.2f   /* sideways speed of the floor-aimed puffs */
#define PARTICLE_FLOOR_DOWN 2.0f
#define PARTICLE_BOUNCE 0.3f         /* scrap restitution, one bounce */
#define PARTICLE_SPIN_MAX 25.0f      /* deg/frame */
#define PARTICLE_WALL_INSET 0.05f
#define TOOTH_COUNT 2
#define TOOTH_LIFE_FRAMES 45
#define TOOTH_UP_SPEED 3.0f
#define TOOTH_OUT_SPEED 1.5f
#define PUFF_SIZE_M 0.12f
#define SCRAP_SIZE_M 0.15f
#define TOOTH_SIZE_M 0.05f
#define PARTICLE_IMG_PX 12.0f        /* nominal source image size the world sizes are scaled from */
#define PARTICLE_MIN_PX 1.0f         /* smaller particles are not drawn */
#define PARTICLE_DRAW_MAX 256        /* farthest particles are dropped above this many */
#define PARTICLE_CMD_RESERVE 240     /* render commands kept free for the impact stars and weapon drawn after particles */
#define BENCH_STORM_PERIOD 60
#define BENCH_STORM_COUNT 6
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
#define PICKUP_AHEAD 3.0f            /* m ahead of the player that a weapon pickup is placed */
#define PICKUP_RADIUS 0.5f           /* walking this close to the pickup opens the offer */
#define PICKUP_ICON_M 0.5f           /* world width of the floor icon */
#define PICKUP_ICON_PX 32.0f         /* source icon size */
#define PICKUP_LIFT_M 0.3f           /* icon centre height above the floor */
#define PICKUP_BOB_PX 3.0f           /* bob amplitude on screen */
#define PICKUP_BOB_RAD_PER_FRAME 0.25f
/* Weapon drawing (screen px, degrees; 0 deg points the weapon straight up). */
/* The weapon turns about a pivot below the screen; held upright, the bat's head is about y 147. */
#define WEAPON_PIVOT_X 200.0f        /* sweep pivot, below the screen */
#define WEAPON_PIVOT_Y 376.0f
#define WEAPON_SWEEP_RADIUS 60.0f    /* grip distance from the pivot */
#define SPEED_LINES 3
#define SPEED_LINE_GAP_DEG 6.0f
#define SPEED_LINE_LEN_DEG 22.0f
#define SPEED_LINE_MIN_VEL 6.0f      /* deg/frame: faster draws speed lines and starts the swing sound */
#define REACH_PATCH_HALF_W 1.2f      /* half width of the floor reach hint, m */
/* Dummy hit boxes, as fractions of the rig height. */
#define HIT_HEAD_HALF_W_FRAC 0.17f
#define HIT_HEAD_H_FRAC 0.34f
#define HIT_ARM_PAD_FRAC 0.12f
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

/* Title, game over and sound. */
#define TITLE_DUMMY_Z 4.0f           /* the idle dummy on the title screen */
#define GAME_OVER_LOCK_FRAMES 15     /* A is ignored this long after death, so a walking player does not skip the screen */
#define HIT_HEAVY_MASS 2.0f          /* heavy hit sound: weapons at least this heavy (the sledge, not the pan at 1.4) */
#define HIT_HEAVY_MOMENTUM 40.0f     /* ... with at least this contact momentum */
#define STEP_SOUND_FRAMES 12         /* walking frames per footstep sound */
#define SOUND_QUEUE_MAX 16

/* Weapon motion (crank-driven, spring-coupled). */
#define CRANK_ZERO_DEG 0.0f          /* crank angle that points the weapon straight up */
#define WEAPON_MAX_VEL 40.0f         /* degrees per frame */
#define WEAPON_BOUNCE_DAMP 0.85f     /* loose weapon velocity multiplier per frame */
#define WEAPON_STOP_VEL 0.5f
#define WEAPON_RECONNECT_DEG 3.0f
#define WEAPON_SHIFT_PX 100.0f
#define WEAPON_SHIFT_PX_PER_FRAME 10.0f
#define WEAPON_REBASE_DEG 3600.0f

/* Weapon contact (swept weapon vs dummy hit boxes). */
#define WEAPON_SWEEP_STEP_DEG 4.0f   /* sample spacing along a frame's sweep */
#define WEAPON_BOUNCE_RESTITUTION 0.4f
#define WEAPON_JAB_PX 45.0f          /* A/B jab: how far the weapon thrusts out along itself */
#define WEAPON_JAB_OUT_FRAMES 3      /* frames to full extension; hits land only while extending */
#define WEAPON_JAB_FRAMES 9          /* out and back */
#define WEAPON_JAB_REACH_M 0.4f      /* extra hit depth at full extension */
#define WEAPON_JAB_MOMENTUM 40.0f    /* momentum per unit of weapon mass */
#define DUMMY_RESIST_SMALL 45.0f     /* momentum a hit needs to carry through */
#define DUMMY_RESIST_MEDIUM 70.0f
#define DUMMY_RESIST_LARGE 130.0f
#define MOMENTUM_REF 80.0f           /* a bat at 20 deg/frame at the tip is about 79.6 */
#define DAMAGE_PER_MOMENTUM (1.0f / MOMENTUM_REF)
#define HIT_SCALE_MIN 0.5f
#define HIT_SCALE_MAX 1.5f

#endif
