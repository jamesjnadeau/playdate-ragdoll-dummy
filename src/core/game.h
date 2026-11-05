#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "camera.h"
#include "render_list.h"
#include "rng.h"
#include "input.h"
#include "bench.h"
#include "hallway.h"
#include "dummy.h"
#include "player.h"
#include "rig.h"
#include "weapon_motion.h"
#include "contact.h"
#include "combat.h"
#include "ragdoll.h"
#include "particles.h"
#include "weapons.h"
#include "save.h"
#include "tuning.h"

typedef enum { MODE_TITLE, MODE_PLAY, MODE_OFFER, MODE_GAME_OVER, MODE_SKINS, MODE_CREDITS } GameMode;

typedef enum { SND_SWING, SND_HIT_LIGHT, SND_HIT_HEAVY, SND_BURST, SND_GRUNT, SND_DOOR_SLAM, SND_STEP, SND_HURT, SND_COUNT } SoundId;

typedef struct { float x, y, r; int t; } Impact;   /* screen star, t = frames left */

typedef struct Game {
    Camera cam;
    RenderList rl;
    Rng rng;
    Hallway hall;
    DummyPool dummies;
    Player player;
    DummyRig rigs[DS_COUNT];
    int pending_damage;   /* damage dealt to the player, consumed by the next player_update */
    int frame;
    Weapon wp;            /* the crank-driven weapon */
    WeaponId weapon;
    GameMode mode;        /* MODE_TITLE, PLAY, OFFER and GAME_OVER are implemented; SKINS and CREDITS come later */
    WeaponId offer;       /* the weapon the pickup offers */
    float pickup_z;       /* -1 when there is no pickup on the floor */
    int offer_doors_seen; /* doors_emptied at the end of the last update, to spot new offers */
    HitResult hits[16];   /* this frame's hits, for effects and sounds */
    int nhits;
    WeaponRig wrigs[WEAPON_COUNT];   /* grip/tip and image size of each held weapon */
    Impact impacts[MAX_IMPACTS];
    RagdollPool ragdolls;
    Vec3 bursts[6];       /* where ragdolls burst this frame */
    Vec3 burst_n[6];      /* surface normal of each burst (away from the wall, floor or ceiling) */
    int nbursts;
    ParticlePool particles;
    BenchScene bench;     /* BENCH_NONE for a normal game */
    int hurt_t;           /* frames of hurt shake left; the frame it starts on also inverts */
    uint8_t held;         /* buttons held in the last update, for the swing-side HUD indicator */
    int bags;             /* this run: knockouts plus bursts */
    float best_distance;  /* metres, from the save file and this run */
    int best_bags;
    int seen_new_best;    /* this run's death set a new best (the game-over screen shows it) */
    bool save_dirty;      /* the platform must write the save file, then clear this */
    bool death_handled;   /* the run's death has been scored */
    uint8_t sounds[SOUND_QUEUE_MAX];   /* SoundId events of this update, for the platform to play; cleared at the start of each update */
    int nsounds;
    bool crank_docked;    /* the crank was docked in the last update (the HUD asks for it to be pulled out) */
    int over_t;           /* frames since death, to ignore A at first on the game-over screen */
    int step_t;           /* walking frames since the last footstep sound */
} Game;

void game_init(Game* g, uint32_t seed);
/* Starts a fresh run in MODE_PLAY. Unlike game_init it keeps the bests and the platform-measured rigs. */
void game_start_run(Game* g, uint32_t seed);
/* Enters the title screen: the hallway with one idle dummy. */
void game_show_title(Game* g);
/* Knocks a dummy out (launch in m/s) and scores its bag, once. */
void game_knock_out(Game* g, int dummy, Vec3 launch);
/* Queues a sound event (dropped when the queue is full). */
void game_push_sound(Game* g, SoundId id);
/* Copies the platform-measured rigs (part sizes) into the game. */
void game_set_rigs(Game* g, const DummyRig rigs[DS_COUNT]);
/* Copies the platform-measured weapon rigs (image sizes) into the game. */
void game_set_weapon_rigs(Game* g, const WeaponRig wrigs[WEAPON_COUNT]);
/* Applies the bests loaded from the save file (call after game_init). Never lowers a best already held. */
void game_apply_bests(Game* g, int best_bags, int best_distance_m);
void game_update(Game* g, const GameInput* in);
/* Starts a benchmark scene. Other scenes behave like WALK until their task. */
void game_start_bench(Game* g, const BenchConfig* cfg);
/* Rebuilds g->rl for the current state. */
void game_render(Game* g);

#endif
