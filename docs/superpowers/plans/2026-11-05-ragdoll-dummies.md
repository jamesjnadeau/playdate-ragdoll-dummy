# Ragdoll Dummies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first release of Ragdoll Dummies, a first-person Playdate brawler in C with mix-and-match skin packs and performance gates enforced by pdsim.

**Architecture:** All game rules live in `src/core/`. This is pure C99 with no Playdate headers and no allocation during play, and it is deterministic for a given seed. Each frame, the core consumes a `GameInput`, then emits a flat `RenderList` of draw commands and a queue of sound events. `src/platform/playdate/` is the only code that touches `pd_api.h`: it fills input, executes the render list, loads packs and plays sounds. Host unit tests cover the core. The playdate-performance-simulator (pdsim) runs the real `pdex.elf` in CI against the Rev A calibration.

**Tech Stack:** C99, Playdate SDK 3.1.2 (`common.mk`), arm-none-eabi-gcc, host gcc + make for tests, Python 3 + Pillow for placeholder art generation, pdsim (Rust, pinned commit `6af8292199ef02a42baf9901719dbb64395e38e8`), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-05-ragdoll-dummy-design.md`. Read it before starting any task. Section numbers such as "§6.7" refer to it.

**Milestones:**
- **A. Foundation:** Tasks 1–3.
- **B. Hallway on screen:** Tasks 4–6.
- **C. Dummies and doors:** Tasks 7–9.
- **D. Swinging and hitting:** Tasks 10–12.
- **E. Ragdolls and bursts:** Tasks 13–14.
- **F. Full run and playtest build:** Tasks 15–18. **Stop after Task 18 for the owner's playtest.**
- **G. Skins:** Tasks 19–22.
- **H. Release gates:** Task 23.

## Global Constraints

- **No doubles:** every float literal has the `f` suffix; use `sinf`/`cosf`/`sqrtf`/`fabsf`/`atan2f`/`fmodf`; no `%f` in any format string.
- **Compiler flags:** both host and device builds add `-Wdouble-promotion -Wfloat-conversion -fsingle-precision-constant -Werror=double-promotion -Werror=float-conversion`.
- **Double guard:** `tools/check_no_doubles.sh build/pdex.elf` must pass. It fails if `arm-none-eabi-nm` lists any symbol matching `__aeabi_d|__adddf3|__subdf3|__muldf3|__divdf3|__extendsfdf2|__truncdfsf2|__floatsidf|__fixdfsi`.
- **Core purity:** files under `src/core/` never include `pd_api.h`, `stdio.h` or `stdlib.h`, and never call `malloc`. `string.h`, `stdint.h`, `stdbool.h`, `stddef.h` and `math.h` are allowed.
- **Fixed step:** the game runs at 30 fps with a fixed `dt = 1.0f/30.0f` per `game_update`. Refresh rate is set with `pd->display->setRefreshRate(30)`.
- **Names:** game name "Ragdoll Dummies", bundle id `com.jamesjnadeau.ragdolldummies`, product `RagdollDummies.pdx`.
- **World units:** metres. x is lateral, from −1.6 (left wall) to +1.6 (right wall). y is up, from 0 (floor) to 2.7 (ceiling). z is distance down the hallway; the player's z only grows when walking forward.
- **Tuning:** every gameplay number lives in `src/core/tuning.h`, and no other file hardcodes one. The initial values are listed in Task 2.
- **Pools:** at most 16 live dummies, 6 ragdolls, 256 particles, 512 render commands, 12 tracked doors and 8 packs.
- **Licensing:** game code is GPL-3.0. The art and sound in `packs/core` are CC BY 4.0, declared in its `pack.json`.
- **Never commit:** the Playdate SDK or a pdsim build. `.pdsim/` and `build/` are gitignored.

## Review Focus

1. **A malformed or truncated community `pack.json`** (bad JSON, a missing field, `spec` greater than 1, a huge file) must be skipped with a logged reason and must never crash the game. Tests: `test_pack_rejects_*` in Task 19.
2. **A pack that is missing an image the selection points at** must fall back to the `core` image for that part, not draw nothing and not crash. Tests: `test_resolve_falls_back_to_core` in Task 19 and the missing-file branch in Task 20.
3. **Crank edge cases:** a docked crank, a huge single-frame crank jump (more than 180°, as when undocking), and direction reversals mid-swing must not trigger double swings or negative speed. Tests: `test_swing_*` in Task 10.
4. **Pool exhaustion:** when 16 dummies, 6 ragdolls or 256 particles are already in use, a new spawn is dropped or the oldest entry is recycled, and nothing is overwritten mid-use. Tests: `test_*_pool_full` in Tasks 8, 13 and 14.
5. **Save file problems** (missing, empty, corrupt, or from an older version) load as defaults, and a new best is still written. Tests: `test_save_*` in Task 16.

---

## File Structure

```
Makefile                         SDK common.mk build (device + simulator) + host targets
Source/pdxinfo                   name/bundle id/version
Source/ui/                       fixed UI images (hearts, bag icon, crank hint) – not skinnable
Source/packs/core/               default pack (pack.json + PNG + WAV), generated then hand-touchable
src/core/
  tuning.h                       every gameplay constant
  rmath.h / rmath.c              Vec2/Vec3, clampf, lerpf, approachf, table trig
  rng.h / rng.c                  xorshift32
  input.h                        GameInput + button bits (same values as PDButtons)
  json.h / json.c                minimal JSON tokenizer + typed getters
  camera.h / camera.c            projection, aim pitch
  render_list.h / render_list.c  draw command buffer
  hallway.h / hallway.c          door generation, door state, pass gating, slam
  dummy.h / dummy.c              dummy pool, AI, punches, stagger, reactions state
  player.h / player.c            movement, blocking, knockback, hearts
  weapons.h / weapons.c          weapon table
  swing.h / swing.c              crank → swing detection, delay, sweep animation state
  combat.h / combat.c            swing → hits, damage, zone, reaction choice
  pose.h / pose.c                procedural reaction poses
  ragdoll.h / ragdoll.c          Verlet ragdolls, collisions, door slams, bowling
  particles.h / particles.c      stuffing + scrap particles
  rig.h                          DummyRig (anchor data from packs)
  draw_world.h / draw_world.c    hallway/dummy/ragdoll/particle/weapon → RenderList
  hud.h / hud.c                  HUD + menus → RenderList
  save.h / save.c                key=value save format
  bench.h / bench.c              bench.json → BenchConfig, scene setup
  pack.h / pack.c                manifest parse/validate, selection, resolution
  game.h / game.c                Game struct, modes, update/render, sound event queue
src/platform/playdate/
  main.c                         eventHandler, update callback
  pd_input.c / .h                PDButtons + crank → GameInput
  pd_render.c / .h               RenderList → pd->graphics; image table
  pd_assets.c / .h               pack discovery + bitmap/sample loading per selection
  pd_sound.c / .h                SoundEvent → sampleplayer
  pd_files.c / .h                read/write whole files
tests/
  test.h                         CHECK/CHECK_NEAR/RUN macros
  main.c                         runs every suite, exits non-zero on failure
  test_<module>.c                one per core module
  fixtures/packs/...             good + broken packs for pack/validator tests
tools/
  check_no_doubles.sh
  make_core_pack.py              generates placeholder art + WAVs for packs/core
  validate_pack.c                host CLI: validate a pack folder
  pdsim.sh                       build pdsim at pinned commit into .pdsim/
  stage_pdsim.sh                 stage pdex.elf + Source assets for pdsim
perf/
  run.sh                         run all scenarios with gates
  scenarios/<name>/bench.json, input.json
docs/pack-spec.md                artist-facing pack spec
.claude/skills/playdate-perf-test/SKILL.md
.github/workflows/ci.yml
```

---

## Milestone A: Foundation

### Task 1: Build skeleton, host test harness, double guard

**Files:**
- Create: `Makefile`, `Source/pdxinfo`, `src/platform/playdate/main.c`, `tests/test.h`, `tests/main.c`, `tests/test_smoke.c`, `tools/check_no_doubles.sh`, `.gitignore`

**Interfaces:**
- Produces:
  - `make device_bin` builds `build/pdex.elf`, and `make` builds `RagdollDummies.pdx`.
  - `make test` builds and runs `build/host/tests`.
  - `tests/test.h` macros: `CHECK(cond)`, `CHECK_EQ_INT(a,b)`, `CHECK_NEAR(a,b,eps)` (floats), `RUN(fn)`. A failure prints the file, line and expression, then increments a global `g_failures`.
  - Each suite is a `void suite_<name>(void)` declared in `tests/main.c`.

- [ ] **Step 1: Write `Makefile`**
  - Copy the SDK Hello World Makefile and set `PRODUCT = RagdollDummies.pdx`.
  - Set `VPATH += src/core:src/platform/playdate` and `UINCDIR = src/core src/platform/playdate`.
  - Set `SRC = $(wildcard src/core/*.c) $(wildcard src/platform/playdate/*.c)`.
  - Set `UDEFS = -std=gnu11 -Wfloat-conversion -fsingle-precision-constant -Werror=double-promotion -Werror=float-conversion`.
  - Add a `test` target. Before the `include` line, add `HOST_CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Wdouble-promotion -Wfloat-conversion -fsingle-precision-constant -Werror -Isrc/core`. The `test` target compiles `src/core/*.c tests/*.c -lm` into `build/host/tests` and runs it.
  - Add a `check-doubles` target that runs `tools/check_no_doubles.sh build/pdex.elf`.
- [ ] **Step 2: Write `Source/pdxinfo`** with `name=Ragdoll Dummies`, `author=James Nadeau`, `bundleID=com.jamesjnadeau.ragdolldummies`, `version=0.1.0`, `buildNumber=1`.
- [ ] **Step 3: Write `main.c`**
  - It handles `kEventInit`: sets the refresh rate to 30, registers the update callback, and the callback clears the screen to white and returns 1.
  - Include a trivial float computation (`sinf(0.5f)`) so the double guard is exercised.
- [ ] **Step 4: Write `tests/test_smoke.c`** with `suite_smoke` containing `CHECK(1 == 1)` and `CHECK_NEAR(0.1f + 0.2f, 0.3f, 1e-6f)`.
- [ ] **Step 5: Run `make test`.** Expected output: `smoke: ok`, exit 0.
- [ ] **Step 6: Write `tools/check_no_doubles.sh`** using the symbol regex from Global Constraints. It prints the offending symbols and exits 1 if any are found.
- [ ] **Step 7: Run `PLAYDATE_SDK_PATH=~/PlaydateSDK make device_bin && make check-doubles`.**
  - Expected: `build/pdex.elf` exists and the script prints `no double-precision helpers`.
  - Then add `volatile double d = 1.0; d *= 2.0;` to `main.c` temporarily, confirm the build fails on `-Werror=double-promotion` (or the guard fails), and remove it.
- [ ] **Step 8: Write `.gitignore`** with `build/`, `*.pdx`, `.pdsim/` and `__pycache__/`.
- [ ] **Step 9: Commit:** `git commit -m "build: SDK makefile, host test harness, no-doubles guard"`

### Task 2: Math, RNG, input, tuning

**Files:**
- Create: `src/core/rmath.h`, `src/core/rmath.c`, `src/core/rng.h`, `src/core/rng.c`, `src/core/input.h`, `src/core/tuning.h`
- Test: `tests/test_rmath.c`, `tests/test_rng.c`

**Interfaces:**
- Produces:
  - `typedef struct { float x, y; } Vec2; typedef struct { float x, y, z; } Vec3;`
  - `float clampf(float v, float lo, float hi); float lerpf(float a, float b, float t); float approachf(float v, float target, float step);`
  - `float rsin_deg(float deg); float rcos_deg(float deg);` use a 1024-entry full-circle sine table built on first call, with linear interpolation; any input angle is wrapped with `fmodf`.
  - `Vec3 v3_add(Vec3, Vec3); Vec3 v3_sub(Vec3, Vec3); Vec3 v3_scale(Vec3, float); float v3_len(Vec3); float v3_dot(Vec3, Vec3);`
  - `typedef struct { uint32_t s; } Rng; void rng_seed(Rng*, uint32_t seed); uint32_t rng_u32(Rng*); int rng_range(Rng*, int lo, int hi_inclusive); float rng_f01(Rng*);` A seed of 0 is mapped to `0x9E3779B9`.
  - `enum { BTN_LEFT=1, BTN_RIGHT=2, BTN_UP=4, BTN_DOWN=8, BTN_B=16, BTN_A=32 };`
  - `typedef struct { uint8_t held, pressed, released; float crank_delta; bool crank_docked; } GameInput;`
  - `tuning.h` as `#define`s with these initial values:

```c
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
```

- [ ] **Step 1: Write failing tests**
  - `test_rmath`:
    - `CHECK_NEAR(rsin_deg(90.0f), 1.0f, 1e-3f)`
    - `CHECK_NEAR(rcos_deg(180.0f), -1.0f, 1e-3f)`
    - `CHECK_NEAR(rsin_deg(-30.0f), -0.5f, 2e-3f)`
    - `CHECK_NEAR(rsin_deg(390.0f), 0.5f, 2e-3f)`
    - `CHECK_NEAR(approachf(1.0f, 0.0f, 0.3f), 0.7f, 1e-6f)`
    - `CHECK_NEAR(approachf(0.1f, 0.0f, 0.3f), 0.0f, 1e-6f)`
    - `CHECK_NEAR(clampf(5.0f, 0.0f, 1.0f), 1.0f, 0)`
  - `test_rng`:
    - Two `Rng` seeded with 42 produce identical first 100 values.
    - `rng_range(&r, 3, 5)` stays within [3,5] over 1000 draws and hits all three values.
    - `rng_f01` stays in [0,1).
    - A seed of 0 is not stuck at zero.
- [ ] **Step 2: Run `make test`.** Expected: link errors or failures for `rsin_deg`, `rng_seed` and the rest.
- [ ] **Step 3: Implement** `rmath.c` and `rng.c` (xorshift32: `x ^= x<<13; x ^= x>>17; x ^= x<<5`), plus `input.h` and `tuning.h` as above.
- [ ] **Step 4: Run `make test`.** Expected: all suites ok.
- [ ] **Step 5: Commit:** `git commit -m "core: math, rng, input, tuning"`

### Task 3: JSON tokenizer, CI for host tests and device build

**Files:**
- Create: `src/core/json.h`, `src/core/json.c`, `.github/workflows/ci.yml`
- Test: `tests/test_json.c`

**Interfaces:**
- Produces:
  - `typedef enum { JT_OBJ, JT_ARR, JT_STR, JT_NUM, JT_TRUE, JT_FALSE, JT_NULL } JsonType;`
  - `typedef struct { JsonType type; int start, end; int size; int parent; } JsonTok;` `size` is the number of children: for an object, the count of key/value pairs.
  - `int json_parse(const char* s, int len, JsonTok* toks, int max_toks);` returns the token count, or -1 for invalid input, -2 when out of tokens. It does not allocate.
  - `int json_obj_get(const char* s, const JsonTok* t, int ntok, int obj, const char* key);` returns the index of the value token or -1.
  - `bool json_str(const char* s, const JsonTok* t, int i, char* out, int cap);` copies the string, handling `\"`, `\\` and `\n`, and truncates safely.
  - `bool json_num_f(const char* s, const JsonTok* t, int i, float* out);` and `bool json_num_i(..., int* out);` are hand-written parsers (no `strtod`, which returns a double).
  - `int json_arr_at(const JsonTok* t, int ntok, int arr, int k);` returns the index of the k-th element or -1.

- [ ] **Step 1: Write failing tests** in `test_json.c`
  - `{"a":1,"b":[2,3],"c":{"d":"x"}}` parses. Then:
    - `json_obj_get(...,"b")` is a `JT_ARR` with `size==2`.
    - `json_arr_at(...,1)` gives 3.
    - The nested `"d"` is `"x"`.
  - `-12.5`, `0.25` and `1e2` parse as floats within `1e-5f`.
  - These all return -1: `{"a":}`, `{"a":1`, `[1,2,,]`, `"unterminated`, the empty string, and an object nested 64 deep when the limit is 32. Depth limit: 32.
  - A document that needs 300 tokens with `max_toks=256` returns -2.
  - `json_str` with `cap=4` on `"hello"` gives `"hel"`.
- [ ] **Step 2: Run `make test`.** Expected: json failures.
- [ ] **Step 3: Implement `json.c`.** Use a single-pass tokenizer with an explicit stack of depth 32, and reject trailing garbage after the root value.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Write `ci.yml`** with two jobs on `ubuntu-24.04`:
  - `host-tests` runs `make test`.
  - `device-build`:
    - Installs `gcc-arm-none-eabi`.
    - Downloads the SDK from `https://download.panic.com/playdate_sdk/Linux/PlaydateSDK-3.1.2.tar.gz` into `$HOME/PlaydateSDK`, and caches it keyed on the version.
    - Runs `make device_bin` and `make check-doubles`.
  - If that URL 404s, check Panic's download page for the current Linux tarball URL pattern, keep the version pinned, and note the URL in the workflow comment.
- [ ] **Step 6: Commit:** `git commit -m "core: json tokenizer; ci: host tests + device build + double guard"`

---

## Milestone B: Hallway on screen

### Task 4: Camera projection and render list

**Files:**
- Create: `src/core/camera.h/.c`, `src/core/render_list.h/.c`
- Test: `tests/test_camera.c`, `tests/test_render_list.c`

**Interfaces:**
- Produces:
  - `typedef struct { float player_z; float pitch_px; int aim; /* 0 low,1 mid,2 high */ } Camera;`
  - `void camera_set_aim(Camera*, int aim); void camera_update(Camera*);` eases `pitch_px` toward the target for the aim level by `AIM_EASE_PX_PER_FRAME`.
  - `bool camera_project(const Camera*, Vec3 w, float* sx, float* sy, float* px_per_m);` returns false when `w.z - player_z < NEAR_Z`.
    - `sx = SCREEN_CX + w.x*FOCAL/dz`
    - `sy = HORIZON_Y + pitch_px - (w.y - CAM_Y)*FOCAL/dz`
    - `px_per_m = FOCAL/dz`
  - Render commands:

```c
typedef enum { RC_POLY, RC_LINE, RC_BITMAP, RC_TEXT, RC_RECT } RcType;
typedef enum { PAT_WHITE, PAT_BLACK, PAT_D25, PAT_D50, PAT_D75 } Pattern;
typedef struct {
  uint8_t type, pattern, npts, flip;  /* flip: 0 none, 1 X */
  int16_t image;                      /* ImageId, -1 none */
  int16_t pts[8];                     /* poly: up to 4 xy pairs; line: x0,y0,x1,y1,width */
  float x, y, angle_deg, cx, cy, sx, sy;  /* bitmap: center-of-rotation normalized cx,cy */
  int16_t text_off;                   /* offset into RenderList.text */
} RenderCmd;
typedef struct { RenderCmd cmd[512]; int n; char text[1024]; int text_n; } RenderList;
void rl_clear(RenderList*);
void rl_poly(RenderList*, const float* xy, int npts, Pattern p);
void rl_line(RenderList*, float x0, float y0, float x1, float y1, int width, Pattern p);
void rl_rect(RenderList*, float x, float y, float w, float h, Pattern fill, bool outline);
void rl_bitmap(RenderList*, int image, float x, float y, float angle_deg, float cx, float cy, float sx, float sy, bool flip_x);
void rl_text(RenderList*, const char* s, float x, float y);
```

  A full list drops new commands silently and increments `RenderList.dropped` (add this `int` field).

- [ ] **Step 1: Write failing tests**
  - `test_project_center`: with `player_z=0, pitch=0`, the point `(0, CAM_Y, 1.5f)` projects to `sx=200, sy=108` and `px_per_m=100`.
  - `test_project_floor_wall`: `(HALL_HALF_W, 0, 3.0f)` gives `sx = 200 + 1.6f*50 = 280` and `sy = 108 + 1.35f*50 = 175.5`.
  - `test_project_behind_near`: `z = player_z + 0.1f` returns false.
  - `test_aim_eases`: `camera_set_aim(&c,0)` (low); after 3 updates `pitch_px == -24`, and after 10 updates it is `-40`.
  - `test_rl_full`: 600 `rl_line` calls give `n==512` and `dropped==88`.
  - `test_rl_text`: two `rl_text` calls store both strings NUL-terminated at distinct offsets. Text that overflows 1024 bytes is dropped, not truncated mid-buffer.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `camera.c` and `render_list.c`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: camera projection, render list"`

### Task 5: Hallway geometry rendering and the Playdate renderer

**Files:**
- Create: `src/core/hallway.h/.c` (geometry part only; doors come in Task 8), `src/core/draw_world.h/.c`, `src/core/game.h/.c` (minimal), `src/platform/playdate/pd_render.c/.h`, `src/platform/playdate/pd_input.c/.h`
- Modify: `src/platform/playdate/main.c`
- Test: `tests/test_draw_world.c`

**Interfaces:**
- Consumes: `Camera`, `RenderList` (Task 4).
- Produces:
  - `void draw_hallway(RenderList*, const Camera*);` draws, in order:
    - walls as `PAT_D25` quads from `NEAR_Z` to `FAR_Z`
    - ceiling `PAT_D50` with white light panels every 3 m (0.7 m wide, 1 m long)
    - white floor with board lines at x = −1.2, −0.8 … 1.2 and cross lines every 1.5 m (on world z multiples of 1.5, so they scroll as the player walks)
    - far darkness: a `PAT_D75` quad at `FAR_Z` with an inner black rect
    - 2 px edge lines where the walls meet the floor and ceiling
    - baseboards at y = 0.25
  - `typedef struct Game Game;` defined in `game.h` with `Camera cam; RenderList rl; Rng rng;` (later tasks add fields). `void game_init(Game*, uint32_t seed); void game_update(Game*, const GameInput*); void game_render(Game*);` For now, update applies A/B walking (`player_z ± PLAYER_SPEED*DT`, clamped ≥ 0) and up/down aim.
  - `void pd_render_execute(PlaydateAPI*, const RenderList*);`
    - Patterns are 8×8 `LCDPattern`s for D25, D50 and D75, matching the mockup.
    - Polys use `fillPolygon` with the `kPolygonFillNonZero` rule.
    - Lines use `drawLine`.
    - Bitmaps use `drawRotatedBitmap`, or `drawScaledBitmap` when `angle_deg == 0` and there is no flip. `flip_x` uses a negative x scale.
    - Text uses `drawText` with the system font.
  - `void pd_input_read(PlaydateAPI*, GameInput*);` fills it from `getButtonState`, `getCrankChange` and `isCrankDocked`.

- [ ] **Step 1: Write failing tests**
  - `test_hallway_emits_geometry`: after `draw_hallway` at `player_z=0`:
    - `rl.n > 20` and `rl.dropped == 0`.
    - The first command is a `RC_POLY` with `PAT_D25` (left wall).
    - Some command is a `RC_RECT` with `PAT_BLACK` whose center is within 2 px of `(200, 108)`.
  - `test_hallway_scrolls`: the y of the nearest floor cross line differs between `player_z=0` and `player_z=0.5f`.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `draw_hallway`, the minimal `game.c`, `pd_render.c`, `pd_input.c`, and wire `main.c`: init → `game_init(seed = pd->system->getCurrentTimeMilliseconds())`; update → read input, `game_update`, `game_render`, `pd_render_execute`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Build `make`, then open the Playdate Simulator** (`~/PlaydateSDK/bin/PlaydateSimulator RagdollDummies.pdx`) if a display is available. Otherwise use pdsim in Task 6. Expected: the hallway looks like mockup A without the dummies or HUD; A walks forward and the floor lines scroll.
- [ ] **Step 6: Commit:** `git commit -m "hallway rendering, playdate renderer and input"`

### Task 6: pdsim integration and bench mode

**Files:**
- Create: `tools/pdsim.sh`, `tools/stage_pdsim.sh`, `src/core/bench.h/.c`, `src/platform/playdate/pd_files.c/.h`, `perf/run.sh`, `perf/scenarios/walk/bench.json`, `perf/scenarios/walk/input.json`, `.claude/skills/playdate-perf-test/SKILL.md`
- Modify: `src/platform/playdate/main.c`, `src/core/game.h/.c`, `.github/workflows/ci.yml`
- Test: `tests/test_bench.c`

**Interfaces:**
- Consumes: `json_*` (Task 3).
- Produces:
  - `typedef enum { BENCH_NONE, BENCH_WALK, BENCH_WAVE, BENCH_BURST_STORM, BENCH_SKINS } BenchScene;`
  - `typedef struct { BenchScene scene; float distance; uint32_t seed; } BenchConfig;`
  - `bool bench_parse(const char* json, int len, BenchConfig* out);`
  - `void game_start_bench(Game*, const BenchConfig*);` Later tasks extend it: for now WALK sets the seed and `player_z=distance`, and the other scenes behave like WALK until their task.
  - `char* pd_read_file(PlaydateAPI*, const char* path, int* len_out);` uses `pd->system->realloc` and `kFileRead|kFileReadData`; the caller frees with `realloc(p, 0)`. `bool pd_write_file(PlaydateAPI*, const char* path, const void* data, int len);`
  - `tools/pdsim.sh`:
    - Clones `https://github.com/jamesjnadeau/playdate-performance-simulator` into `${PDSIM_DIR:-.pdsim/src}` at commit `6af8292199ef02a42baf9901719dbb64395e38e8`.
    - Builds with `cargo build --release -p pdsim-cli`.
    - Prints the binary path. It is idempotent.
  - `tools/stage_pdsim.sh` creates `build/pdsim-stage/` holding `pdex.elf` plus a copy of `Source/` contents (PNG, JSON, WAV), because pdsim reads PNG next to `pdex.elf`.
  - `perf/run.sh [scenario...]` does the following for each scenario:
    - Makes a fresh data dir and copies `bench.json` into it.
    - Runs `pdsim run build/pdsim-stage --frames 600 --input perf/scenarios/<s>/input.json --calibration .pdsim/src/calibration/rev-a.json --data-dir <dir> --report build/perf/<s>.json --frames-out build/perf/<s>.jsonl --screenshot build/perf/<s>.png --max-over-budget 0 -q`.
    - Exits non-zero on any failure.
    - Per-scenario `--max-avg-ms` comes from `perf/scenarios/<s>/max_avg_ms`, and is skipped while that file is absent.

- [ ] **Step 1: Write failing tests** in `test_bench.c`
  - `{"scene":"wave","distance":400,"seed":7}` gives `BENCH_WAVE`, 400, 7.
  - Missing keys default to `BENCH_WALK`, 0 and seed 1.
  - An unknown scene name or invalid JSON returns false.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `bench.c`, `pd_files.c`, and in `main.c` read `bench.json` at init. On success, call `game_start_bench` and skip the title. The title arrives in Task 17; until then the game starts directly.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Write the walk scenario.**
  - `bench.json` is `{"scene":"walk","seed":1}`.
  - `input.json` holds A from frame 5 to 590 and adds crank bursts of `{"frame":N,"crank":25.0}` every 30 frames from frame 30.
- [ ] **Step 6: Run `tools/pdsim.sh && make device_bin && tools/stage_pdsim.sh && perf/run.sh walk`.**
  - Expected: exit 0, a summary showing 0 over-budget frames, and `build/perf/walk.png` showing the hallway when opened with the Read tool. **Also confirm here that pdsim resolves extension-less `loadBitmap` paths to `.png`.** Load one test image from `Source/ui/` (create a 16×16 `Source/ui/test.png`) and check that it appears in the screenshot. If it doesn't, add a `.png`-fallback in `pd_assets` for pdsim only: try the path, then the path + `.png`.
- [ ] **Step 7: Copy the skill.**
  - Copy `.claude/skills/playdate-perf-test/SKILL.md` from the pdsim repo verbatim.
  - Append a "This repo" section: build pdsim with `tools/pdsim.sh`, stage with `tools/stage_pdsim.sh`, run `perf/run.sh <scenario>`, use the `rev-a` profile (the owner has a Rev A), and place scenarios in `perf/scenarios/`.
- [ ] **Step 8: Add CI job `perf`** (needs `device-build`):
  - Installs Rust.
  - Caches `.pdsim/` keyed on the pinned commit.
  - Runs the Step 6 command.
  - Uploads `build/perf/` as an artifact.
- [ ] **Step 9: Commit:** `git commit -m "perf: pdsim harness, bench mode, walk scenario, perf skill"`

---

## Milestone C: Dummies and doors

### Task 7: Core pack placeholder art and the dummy rig

**Files:**
- Create: `tools/make_core_pack.py`, `Source/packs/core/` (generated), `src/core/rig.h`, `src/platform/playdate/pd_assets.c/.h` (core pack only; Task 20 adds community packs)
- Test: `tests/test_rig.c`

**Interfaces:**
- Produces:
  - Image ids, in `rig.h`:

```c
typedef enum { DS_SMALL, DS_MEDIUM, DS_LARGE, DS_COUNT } DummySize;
typedef enum { PART_BASE, PART_BODY, PART_HEAD, PART_ARM_UPPER, PART_ARM_LOWER, PART_FIST,
               PART_FACE_MEAN, PART_FACE_HURT, PART_FACE_DAZED, PART_FACE_KO,
               PART_FACE_WHACK, PART_FACE_SQUISH, PART_COUNT } DummyPart;
#define WEAPON_COUNT 4
#define STUFF_PUFF_VARIANTS 4
#define STUFF_SCRAP_VARIANTS 4
enum {
  IMG_DUMMY0 = 0,                                     /* + size*PART_COUNT + part */
  IMG_WEAPON_HELD0 = IMG_DUMMY0 + DS_COUNT*PART_COUNT, /* + weapon */
  IMG_WEAPON_ICON0 = IMG_WEAPON_HELD0 + WEAPON_COUNT,
  IMG_PUFF0 = IMG_WEAPON_ICON0 + WEAPON_COUNT,
  IMG_SCRAP0 = IMG_PUFF0 + STUFF_PUFF_VARIANTS,
  IMG_UI_HEART_FULL = IMG_SCRAP0 + STUFF_SCRAP_VARIANTS, IMG_UI_HEART_EMPTY, IMG_UI_BAG,
  IMG_COUNT
};
static inline int img_dummy(DummySize s, DummyPart p) { return IMG_DUMMY0 + (int)s*PART_COUNT + (int)p; }
```

  - The rig: anchor points are in pixels relative to the base center, with y negative upward, matching the spec §8.3 `anchors`.

```c
typedef struct {
  float height_px;
  Vec2 neck, shoulder_l, shoulder_r, face_center, body_center;
  float upper_arm_px, lower_arm_px;
  int16_t part_w[PART_COUNT], part_h[PART_COUNT];  /* filled by platform from loaded bitmaps */
} DummyRig;
typedef struct { Vec2 grip, tip; int16_t w, h; } WeaponRig;
void rig_default(DummySize s, DummyRig* out);  /* core pack anchors scaled by size */
```

  - `bool pd_assets_load_core(PlaydateAPI*, LCDBitmap* images[IMG_COUNT], DummyRig rigs[DS_COUNT], WeaponRig wrigs[WEAPON_COUNT]);` loads `packs/core/<folder>/<file>` (without extension) and fills `part_w/h` with `getBitmapData`. A missing optional face falls back as follows: whack → hurt, squish → dazed.
  - `make_core_pack.py` (Pillow) draws the mockup art style as 1-bit RGBA PNGs: black, white and transparent only.
    - **Medium dummy** (`height_px = 120`):
      - body 46×72
      - head 38×38
      - arm-upper 11×22 (pivot at the top center)
      - arm-lower 11×20
      - fist 20×20
      - base 58×22
      - faces 34×30 each, drawn like the mockup: mean, hurt, dazed, ko, whack, squish
    - **Small and large dummies** are the same drawings scaled to `height_px` 84 and 156.
    - **Weapons, `held.png`**, each about 60×190 with the grip near the bottom, in folders `weapon-chicken`, `weapon-bat`, `weapon-pan` and `weapon-sledge` (these names match `WeaponDef.folder` and the slot keys):
      - rubber chicken
      - bat
      - frying pan
      - sledgehammer
    - **Weapon icons:** 32×32.
    - **Stuffing:** 4 puffs (8–14 px) and 4 scraps (10–18 px, jagged with a dashed seam).
    - **`pack.json`:** follows spec §8.3, with `id:"core"`, `license:"CC-BY-4.0"` and anchors for all three sizes and four weapons.
    - **Sounds:** synthesized 22 kHz 16-bit mono WAVs:
      - `swing.wav`: filtered noise sweep
      - `hit-light.wav`, `hit-heavy.wav`: thumps
      - `burst.wav`: noise burst
      - `grunt-1.wav` to `grunt-3.wav`: pitched blips
      - `door-slam.wav`
      - `step.wav`
      - `hurt.wav`: the player-hurt "oof" (added in the playtest tuning pass)
    - **UI images** in `Source/ui/`: heart-full/empty (14×12) and a bag icon (16×16).

- [ ] **Step 1: Write failing test** `test_rig_default`:
  - The medium rig has `height_px == 120`.
  - The large rig's `shoulder_l.x` is 1.3× the medium one (156/120) within 1e-4.
  - The small rig's `neck.y` is less than 0 and greater than `-height_px`.
- [ ] **Step 2: Run `make test`.** Expected: failure.
- [ ] **Step 3: Implement** `rig_default` with medium anchors `neck(0,-78) shoulder_l(-20,-64) shoulder_r(20,-64) face_center(0,-94) body_center(0,-45) upper_arm_px 20 lower_arm_px 18`, scaled by `height_px/120`.
- [ ] **Step 4: Write and run `python3 tools/make_core_pack.py`.** Expected: every file listed above exists under `Source/packs/core/` and `Source/ui/`. Check by viewing one PNG of each kind with the Read tool.
- [ ] **Step 5: Implement `pd_assets_load_core`** and log any missing file with `pd->system->logToConsole`.
- [ ] **Step 6: Run `make test`.** Expected: ok. Then run `make` and confirm the build succeeds.
- [ ] **Step 7: Commit:** `git commit -m "core pack placeholder art generator, rig, asset loading"`

### Task 8: Doors, dummy pool, emerging and walking

**Files:**
- Modify: `src/core/hallway.h/.c`, `src/core/game.h/.c`
- Create: `src/core/dummy.h/.c`
- Test: `tests/test_hallway.c`, `tests/test_dummy.c`

**Interfaces:**
- Produces:

```c
typedef enum { DOOR_CLOSED_FULL, DOOR_OPEN, DOOR_CLOSED_DONE } DoorState;
typedef struct { int side; /* -1 left, +1 right */ float z; DoorState state; int remaining; int spawn_timer; float leaf_angle; } Door;
typedef struct { Door doors[12]; int ndoors; float next_z; int doors_emptied; } Hallway;
void hallway_init(Hallway*, Rng*);
int  hallway_dummies_for_door(float z);       /* 1 + (int)(z/60), max 5 */
DummySize hallway_pick_size(Rng*, float z);    /* small only < 20 m; medium from 20 m; large from 60 m; equal weights among allowed */
void hallway_update(Hallway*, Rng*, float player_z); /* opens doors within DOOR_ACTIVATE_DIST, generates doors ahead up to player_z+FAR_Z, recycles doors more than 4 m behind */

typedef enum { DST_EMERGING, DST_WALK, DST_WINDUP, DST_PUNCH, DST_STAGGER, DST_RAGDOLL, DST_DEAD } DummyState;
typedef struct {
  bool alive; DummySize size; DummyState state; int timer;
  float x, z, hp; int door;          /* door index it came from, -1 none */
  int reaction; int reaction_t;      /* ReactionType from Task 11, frames left */
  int ragdoll;                       /* ragdoll index once knocked out, -1 */
} Dummy;
typedef struct { Dummy d[16]; } DummyPool;
int  dummy_spawn(DummyPool*, DummySize, float x, float z, int door); /* returns index or -1 if full */
float dummy_height(DummySize); float dummy_radius(DummySize); float dummy_speed(DummySize);
void dummy_update_all(DummyPool*, float player_z, float player_x, int* out_damage_to_player); /* AI, separation, windup, punch */
```

- Door spawning lives in `game_update`. When an `DOOR_OPEN` door has `remaining > 0`, `spawn_timer` counts down from `DOOR_SPAWN_INTERVAL`. At 0 it spawns a dummy at `x = side*(HALL_HALF_W-0.3f), z = door.z + DOOR_LEN/2` in `DST_EMERGING` for `DUMMY_EMERGE_FRAMES` frames, then decrements `remaining`.
- Dummy AI in `DST_WALK`:
  - It moves toward `(player_x, player_z + 0.1f)` at its size speed.
  - It never moves behind the player's z: it stops at `player_z + PLAYER_RADIUS + radius`.
  - It pushes apart from other dummies so no two centers are closer than the sum of their radii.
  - It stays within the walls.
- Punch cycle:
  - Within `DUMMY_PUNCH_REACH` of the player in z, a walking dummy enters `DST_WINDUP` for its size's windup frames.
  - Then it enters `DST_PUNCH` for 4 frames. Damage is added on the punch's first frame.
  - Then it returns to `DST_WALK` after a 20-frame cooldown (`timer`).

- [ ] **Step 1: Write failing tests**
  - `test_door_counts`:
    - `hallway_dummies_for_door(0)==1`, `(59)==1`, `(60)==2`, `(10000)==5`.
    - `hallway_pick_size` at z=10 always returns small over 200 draws.
    - At z=100, all three sizes appear.
  - `test_doors_generated_ahead`: after `hallway_update` at `player_z=0`, every door z is ≥ `DOOR_FIRST_Z`, gaps are within [3,6] m plus `DOOR_LEN`, and the last door z is ≥ `FAR_Z - DOOR_GAP_MAX`.
  - `test_door_opens_when_near`: a door at z=6 is `DOOR_CLOSED_FULL` at `player_z=-3` and `DOOR_OPEN` at `player_z=-1.9f`.
  - `test_dummy_pool_full`: 16 spawns succeed, the 17th returns -1, and no existing dummy changes.
  - `test_dummy_walks_toward_player`: after 30 updates, a medium dummy at (0, 5) is at z = 5 − 0.75 within 0.02.
  - `test_dummy_stops_in_front`: it never ends with z < `player_z + PLAYER_RADIUS + DUMMY_RADIUS_MEDIUM - 1e-4f`.
  - `test_dummy_separation`: two dummies spawned at the same spot are at least `2*radius - 1e-3f` apart after 10 updates.
  - `test_dummy_punch_damage`: a large dummy placed in reach deals exactly 2 damage, once, after 24 windup frames plus 1, and then no more for 20 frames.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `hallway.c` (doors) and `dummy.c`, and wire spawning into `game_update`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: doors, dummy pool, AI and punches"`

### Task 9: Player blocking, door gating and slams, drawing dummies

**Files:**
- Create: `src/core/player.h/.c`, `src/core/pose.h/.c` (the idle/walk/windup poses only; reactions come in Task 11)
- Modify: `src/core/draw_world.h/.c`, `src/core/game.h/.c`, `src/core/hallway.h/.c`
- Test: `tests/test_player.c`, `tests/test_draw_world.c`

**Interfaces:**
- Consumes: `DummyPool`, `Hallway`, `DummyRig` (Tasks 7 and 8).
- Produces:

```c
typedef struct { float z, x; int hearts; int invuln; int knock_frames; float knock_v; bool dead; } Player;
void player_init(Player*);
typedef enum { PEV_NONE = 0, PEV_DOOR_SLAM = 1, PEV_DOOR_CLOSED = 2, PEV_HURT = 4 } PlayerEvent;
int  player_update(Player*, const GameInput*, Hallway*, DummyPool*, int incoming_damage); /* returns PlayerEvent bits */

typedef struct {
  float lean_deg, sx, sy, lift_m;          /* whole-body */
  float head_dx, head_dy, head_rot_deg, head_sy; /* head offset in rig px */
  float arm_l_deg, arm_r_deg, elbow_l_deg, elbow_r_deg; /* 0 = hanging down, + = outward */
  DummyPart face;
} DummyPose;
void pose_for_state(const Dummy*, int frame, DummyPose* out); /* walk waddle (lean ±4° at 2 Hz), windup (arms back, lean back 8°), punch (arm forward) */
void draw_dummy(RenderList*, const Camera*, const Dummy*, const DummyRig*, const DummyPose*);
void draw_door(RenderList*, const Camera*, const Door*);
```

- Player movement:
  - A moves +z and B moves −z at `PLAYER_SPEED`. z never goes below the last `DOOR_CLOSED_DONE` door z, so you can't walk back past a closed door.
  - **Blocking:** forward movement stops at `min(dummy.z - dummy_radius - PLAYER_RADIUS)` over live non-ragdoll dummies whose |x − player.x| < radius + 0.6 m. The player's x is fixed at 0. Dummies always path to the player, so any dummy ahead in the lane blocks.
  - **Door gating (§6.7):**
    - When the player's z crosses `door.z + DOOR_LEN/2` while the door is `DOOR_OPEN` or `DOOR_CLOSED_FULL` with `remaining > 0`, or a dummy from that door is still `DST_EMERGING`, a slam happens:
      - The player is pushed back `KNOCKBACK_DIST` over `KNOCKBACK_FRAMES` (no heart loss).
      - The door spawns its next dummy immediately (the timer is set to 0).
      - The update returns `PEV_DOOR_SLAM`.
    - When it crosses with `remaining == 0` and no emerging dummy, the door becomes `DOOR_CLOSED_DONE`, `hallway.doors_emptied++`, and the update returns `PEV_DOOR_CLOSED`.
  - **Damage:** `incoming_damage > 0` while `invuln == 0` subtracts hearts, sets `invuln = PLAYER_INVULN_FRAMES` and returns `PEV_HURT`. At 0 hearts, `dead = true`.
- Drawing a dummy:
  - **Placement:** the base sits at `camera_project((x, 0, z))`, and `scale = px_per_m * dummy_height(size) / rig.height_px`.
  - **Part order:**
    1. base
    2. body, centered on `body_center`
    3. arms: upper from the shoulder, rotated by the arm angle, pivot (0.5, 0); lower from the elbow; fist at the hand
    4. head at `neck + (head_dx, head_dy)` with `head_rot`/`head_sy`
    5. face at `face_center` + the head offset
  - **Pose transforms:** apply lean as a rotation of every anchor around the base. `sx`/`sy` scale anchor offsets and part scales. `lift_m` raises the base.
  - Sort dummies far to near before drawing.

- [ ] **Step 1: Write failing tests**
  - `test_player_walks`: holding A for 30 frames with no dummies moves z by `PLAYER_SPEED` within 0.01.
  - `test_player_blocked_by_dummy`: a medium dummy at z=2 stops the player at `2 - 0.38f - 0.35f` within 0.01.
  - `test_door_slam`: with a door at z=4 (`remaining=2`, `DOOR_OPEN`) and no dummies, walking past `4.55` returns `PEV_DOOR_SLAM`. 6 frames later the player's z is about 1 m behind the slam point, and a new dummy exists in `DST_EMERGING` with `door==0`.
  - `test_door_closes_when_empty`: with `remaining=0`, walking past returns `PEV_DOOR_CLOSED`, the state becomes `DOOR_CLOSED_DONE`, and `doors_emptied==1`.
  - `test_cant_walk_back_past_closed_door`: after that, holding B never takes z below 4.
  - `test_hurt_and_invuln`: two damage events on consecutive frames remove 1 heart total. Hearts at 1 with damage 2 gives `dead`.
  - `test_draw_dummy_scales_with_distance`: a medium dummy at dz=2 produces body `RC_BITMAP` `sy` values exactly double those at dz=4.
  - `test_dummies_sorted_far_to_near`: the first body bitmap belongs to the farther dummy.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `player.c`, `pose.c` (state poses), `draw_dummy` and `draw_door`, then wire them into `game_update` and `game_render`. The draw order follows §7.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Run `make device_bin && tools/stage_pdsim.sh && perf/run.sh walk`.** Expected: the screenshot shows dummies coming out of doors and the run passes the budget.
- [ ] **Step 6: Commit:** `git commit -m "player movement, blocking, door slams, dummy drawing"`

---

## Milestone D: Swinging and hitting

### Task 10: Weapons and crank swing detection

**Files:**
- Create: `src/core/weapons.h/.c`, `src/core/swing.h/.c`
- Test: `tests/test_swing.c`

**Interfaces:**
- Produces:

```c
typedef enum { W_CHICKEN, W_BAT, W_PAN, W_SLEDGE } WeaponId;
typedef struct { const char* name; const char* folder; int delay; float power, reach, arc, launch; int speed_bars, power_bars; } WeaponDef;
const WeaponDef* weapon_def(WeaponId);
/* values: chicken {"Rubber Chicken","weapon-chicken",2,0.6f,1.3f,1.0f,3.0f,6,1}
           bat     {"Bat","weapon-bat",5,1.0f,1.6f,1.2f,5.0f,4,3}
           pan     {"Frying Pan","weapon-pan",7,1.4f,1.4f,1.0f,6.0f,3,4}
           sledge  {"Sledgehammer","weapon-sledge",12,2.4f,1.8f,1.4f,9.0f,1,6} */

typedef enum { SIDE_LEFT = -1, SIDE_CENTER = 0, SIDE_RIGHT = 1 } SwingSide;
typedef enum { VERT_UP = -1, VERT_DOWN = 1 } SwingVert;
typedef enum { SWS_READY, SWS_MEASURE, SWS_DELAY, SWS_SWEEP, SWS_RECOVER } SwingPhase;
typedef struct { SwingPhase phase; int t; SwingSide side; SwingVert vert; float peak; float factor; float sweep01; } Swing;
typedef enum { SWE_NONE, SWE_STARTED, SWE_IMPACT } SwingEvent;
SwingEvent swing_update(Swing*, const GameInput*, const WeaponDef*);
```

- Rules:
  - **Docked crank:** while `crank_docked`, the swing stays `SWS_READY`.
  - **Clamping:** clamp `crank_delta` to ±90 per frame before use, so an undock jump or a full spin can't explode the speed.
  - **Start:**
    - In `SWS_READY`, |delta| ≥ `SWING_TRIGGER_DEG` starts `SWS_MEASURE`.
    - The side comes from the held D-pad left/right, or center.
    - The vertical comes from the delta sign: positive (clockwise) is `VERT_DOWN`, negative is `VERT_UP`.
    - The update returns `SWE_STARTED`.
  - **Measuring:**
    - `SWS_MEASURE` lasts `SWING_WINDOW` frames, tracking the peak |delta| in the starting direction. Deltas in the opposite direction are ignored.
    - Then `factor = clamp(peak/SWING_SPEED_REF, SWING_FACTOR_MIN, SWING_FACTOR_MAX)`.
  - **Sequence:** `SWS_DELAY` runs `weapon.delay` frames, then `SWS_SWEEP` runs `SWING_SWEEP_FRAMES` frames with `sweep01` going 0→1. `SWE_IMPACT` is returned on the sweep frame where `sweep01` first reaches ≥ 0.5. Then `SWS_RECOVER` runs `SWING_RECOVER_FRAMES` frames and the swing returns to `SWS_READY`.
  - **No overlap:** crank input during anything other than `SWS_READY` and `SWS_MEASURE` is ignored, so there are no double swings.

- [ ] **Step 1: Write failing tests**
  - `test_swing_basic_timing`: with the bat and `delta=+30` for 4 frames, `SWE_STARTED` is on frame 1, side center, `VERT_DOWN`, factor 1.0. `SWE_IMPACT` happens exactly once, at frame `1 + 4 + 5 + 3` (with SWEEP 6, 0.5 is reached on the 3rd sweep frame). Back in `SWS_READY` by frame `1+4+5+6+8`.
  - `test_swing_side_and_up`: holding `BTN_LEFT` with `delta=-20` gives `SIDE_LEFT`, `VERT_UP`, factor `0.6667` within 1e-3.
  - `test_swing_docked_ignored`: with `crank_docked=true` and `delta=50`, there is never `SWE_STARTED`.
  - `test_swing_huge_jump_clamped`: `delta=+400` for one frame, then 0, gives `factor == SWING_FACTOR_MAX`.
  - `test_swing_no_double`: continuous `delta=+30` for 60 frames with the chicken (21-frame cycle: 1 start + 4 measure + 2 delay + 6 sweep + 8 recover) gives exactly 3 impacts, on frames 10, 31 and 52.
  - `test_swing_reversal_ignored_in_measure`: frames +30, −40, −40, −40 give factor 1.0 and `VERT_DOWN`.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `weapons.c` and `swing.c`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: weapons table, crank swing detection"`

### Task 11: Combat resolution and reactions

**Files:**
- Create: `src/core/combat.h/.c`
- Modify: `src/core/pose.h/.c`, `src/core/dummy.h/.c`, `src/core/game.h/.c`
- Test: `tests/test_combat.c`, `tests/test_pose.c`

**Interfaces:**
- Consumes: `Swing`, `WeaponDef` (Task 10), `DummyPool` (Task 8), `Camera.aim`.
- Produces:

```c
typedef enum { RX_NONE, RX_FROM_LEFT, RX_FROM_RIGHT, RX_UPWARD, RX_DOWNWARD,
               RX_LEFT_UP, RX_LEFT_DOWN, RX_RIGHT_UP, RX_RIGHT_DOWN } ReactionType;
typedef struct { int dummy; float damage; bool head; bool knockout; ReactionType rx; Vec3 launch; } HitResult;
int combat_resolve(const Swing*, const WeaponDef*, int aim, float player_z, DummyPool*, HitResult* out, int max_out);
ReactionType combat_reaction(SwingSide, SwingVert);
void pose_for_reaction(ReactionType, float t01, const Dummy*, DummyPose* out);
```

- Combat rules:
  - **Reach and arc:** a dummy is hit when it is not ragdolled and not emerging, `dz = z - player_z` is in (0, reach], and `|x - side*0.6f| <= arc`, where `side` is the swing side as −1/0/1.
  - **Zone:** head when `aim == head_aim(size)`; small → 0, medium → 1, large → 2. Body otherwise.
  - **Damage:** `power * factor * (head ? HEAD_MULT : 1)`.
  - **Effects:** each hit sets `reaction = combat_reaction(...)`, `reaction_t = REACTION_FRAMES`, enters `DST_STAGGER` for `DUMMY_STAGGER_FRAMES`, and pushes z back by `DUMMY_HIT_PUSHBACK * factor`.
  - **Knockout:** when hp ≤ 0, `knockout = true` and `launch = normalize(−side*0.5f, vert==VERT_UP ? 0.8f : 0.3f, 0.9f) * weapon.launch * factor`. A center swing has x = 0, and the dummy flies away from the swing side.
  - **Reaction mapping:** center + up → `RX_UPWARD`, center + down → `RX_DOWNWARD`, left + up → `RX_LEFT_UP`, and so on. "From left" means `SIDE_LEFT`; the dummy leans to the screen-right.
- Poses at the peak (t01 = 1 at the start, easing to 0 with `t01*t01`), from spec §6.5 and the mockups:
  - **`RX_FROM_LEFT`:** lean +20°, arms flung (arm_l 145°, arm_r −30°), face hurt.
  - **`RX_FROM_RIGHT`:** the mirror.
  - **`RX_UPWARD`:**
    - The head snaps back: `head_dy = −18 px`, `head_rot = −17°`, `head_sy = 0.82`.
    - Body lean −6°, arms up 130°, face whack.
    - The flag `teeth = true` goes in `DummyPose` (add a `bool teeth` field) so draw_world spawns two tooth particles.
  - **`RX_DOWNWARD`:** `sx = 1.28`, `sy = 0.72`, arms 75° with elbows −35°, face squish, and `stars = true` (add the field).
  - **Diagonals:** the sum of the side lean and the vertical head/squash parts.

- [ ] **Step 1: Write failing tests**
  - `test_hit_in_reach`: bat (reach 1.6), center swing, a medium dummy at dz=1.2, x=0 gives 1 hit with damage 1.0 (aim 0, so body).
  - `test_head_zone_by_aim`: the same setup with aim 1 gives `head` and damage 1.5.
  - `test_out_of_reach`: dz=1.7 gives 0 hits.
  - `test_side_arc`: a left swing hits a dummy at x=−1.5 (|−1.5+0.6|=0.9 ≤ 1.2) but misses one at x=+1.5 (2.1 > 1.2).
  - `test_knockout_launch`: the sledge (factor 1.5) at a small dummy with hp 2 knocks it out with `launch.z > 0`. For a left swing, `launch.x > 0`, so it flies away from the swing side.
  - `test_reaction_mapping`: all 6 combinations map as listed.
  - `test_pose_upward_head_back`: at t01=1, `pose.head_dy == -18` and `face == PART_FACE_WHACK`. At t01=0 it equals the neutral pose.
  - `test_pose_from_left_mirror`: `FROM_LEFT.lean_deg == -FROM_RIGHT.lean_deg`.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `combat.c` and the reaction poses. In `game_update`, call `combat_resolve` on `SWE_IMPACT`. Reactions override state poses while `reaction_t > 0`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: hit resolution, damage zones, reaction poses"`

### Task 12: Drawing the weapon sweep and impact effects

**Files:**
- Modify: `src/core/draw_world.h/.c`, `src/core/game.h/.c`
- Test: `tests/test_draw_world.c`

**Interfaces:**
- Produces:
  - `void draw_weapon(RenderList*, const Swing*, const WeaponRig*, int image);`
    - **At rest:** the weapon sits at the bottom right. The grip is drawn at screen (352, 262), rotated −53° (leaning up-left as in mockup A), pivot = grip normalized within the image.
    - **During the sweep:** it rotates around a pivot at (200, 300), below the screen. The angle goes `start → end` by `sweep01`:
      - left: −70° → +70°
      - right: +70° → −70°
      - center up: 160° → 20° (bottom to top)
      - center down: 20° → 160°
      - 0° points straight up.
    - Speed lines: 3 `rl_line`s behind the leading edge while sweeping.
    - **No hands are drawn** (Decided).
  - `void draw_impact(RenderList*, float x, float y, float r);` draws a white 14-point star with a 2 px outline, shown for 4 frames at the projected head or body point of each hit.
  - Screen shake: when `PEV_HURT`, `game_render` offsets all world commands by ±3 px for 6 frames and inverts for 1 frame. Add `bool invert` to RenderList, executed by the platform with `pd->display->setInverted`.

- [ ] **Step 1: Write failing tests**
  - `test_weapon_rest_pose`: in `SWS_READY`, `draw_weapon` emits exactly one `RC_BITMAP` with `angle_deg == -53`.
  - `test_weapon_sweep_left_to_right`: a left swing at `sweep01` 0 and 1 gives angles −70 and +70, measured from the emitted bitmap.
  - `test_weapon_sweep_up`: a center up swing at `sweep01=0.5` gives angle 90 (pointing sideways mid-arc).
  - `test_hurt_shake`: after a `PEV_HURT`, `rl.invert` is true for one rendered frame only.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** the drawing, and the `invert` handling in `pd_render.c`.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Add a scenario.**
  - Create `perf/scenarios/fight/` with `bench.json` `{"scene":"walk","seed":3}` and an input that holds A and alternates crank bursts with left and right holds every 25 frames.
  - Run `perf/run.sh fight`. Expected: pass, and the screenshot shows the weapon mid-sweep or dummies reacting. If the last frame misses it, adjust `--frames`.
- [ ] **Step 6: Commit:** `git commit -m "weapon sweep, impact stars, hurt shake"`

---

## Milestone E: Ragdolls and bursts

### Task 13: Verlet ragdolls, collisions, bowling and door slams

**Files:**
- Create: `src/core/ragdoll.h/.c`
- Modify: `src/core/game.h/.c`, `src/core/draw_world.h/.c`
- Test: `tests/test_ragdoll.c`

**Interfaces:**
- Consumes: `HitResult.launch` (Task 11), `DummyPool`, `Hallway`.
- Produces:

```c
enum { RP_BASE, RP_BODY_LO, RP_BODY_HI, RP_HEAD, RP_ELBOW_L, RP_FIST_L, RP_ELBOW_R, RP_FIST_R, RP_COUNT };
typedef struct { bool active; DummySize size; Vec3 p[RP_COUNT], prev[RP_COUNT]; float rest[RP_COUNT+2]; float hp; int still_frames; int dummy; } Ragdoll;
typedef struct { Ragdoll r[6]; } RagdollPool;
int  ragdoll_spawn(RagdollPool*, const Dummy*, Vec3 launch);  /* oldest recycled if full; returns index */
typedef struct { int burst_count; int burst_idx[6]; Vec3 burst_at[6]; int bowl_hits; } RagdollStepOut;
void ragdoll_step(RagdollPool*, DummyPool*, Hallway*, RagdollStepOut* out);
void draw_ragdoll(RenderList*, const Camera*, const Ragdoll*, const DummyRig*);
```

- **Rest pose:** the points come from the dummy's rig anchors converted to metres (`height_m / height_px`), placed at the dummy's x and z. All points get `prev = p - launch*DT`, so they start with the launch velocity. The body points get 100% of the velocity, the head 110%, and the fists 120%, for floppiness.
- **Constraints:** sticks are base–body_lo, body_lo–body_hi, body_hi–head, body_hi–elbow_l, elbow_l–fist_l, body_hi–elbow_r, elbow_r–fist_r, plus two stiffeners: base–body_hi and body_lo–head. Each step does `RAGDOLL_ITER` iterations.
- **Gravity:** `GRAVITY`.
- **Collision:** with the planes x = ±HALL_HALF_W, y = 0 and y = HALL_H. On contact, the normal velocity is reflected × `RAGDOLL_RESTITUTION` and the tangential velocity × `RAGDOLL_FRICTION`.
  - If the normal impact speed > `RAGDOLL_IMPACT_MIN`, `hp -= (speed - RAGDOLL_IMPACT_MIN)`.
  - At `hp <= 0`, the ragdoll bursts: it is reported in `out`, deactivated, and its dummy is set to `DST_DEAD`.
- **Bowling (Decided):** any ragdoll point moving faster than `RAGDOLL_BOWL_MIN` m/s inside a live dummy's cylinder (radius, height) applies the following to that dummy:
  - damage `RAGDOLL_BOWL_DMG * speed`
  - stagger
  - a reaction from the relative x direction
  - a push of `0.3f` m along the ragdoll's velocity

  If that knocks the dummy out, it ragdolls too, with launch = 0.6 × the hitting point's velocity. Each ragdoll–dummy pair hits at most once every 10 frames.
- **Door slam (Decided):** a ragdoll point that touches the wall within an `DOOR_OPEN` door's z-span at speed > `RAGDOLL_IMPACT_MIN` sets the door to `DOOR_CLOSED_FULL` with `leaf_angle = 0`. Its `remaining` is unchanged, and it reopens when the player is within `DOOR_ACTIVATE_DIST` again (next `hallway_update`). A dummy that is `DST_EMERGING` from that door takes `RAGDOLL_DOORSLAM_DMG` and is staggered.
- **Settling:** a ragdoll with every point slower than 0.2 m/s for `RAGDOLL_REST_FRAMES` frames deactivates (fades), and its dummy becomes `DST_DEAD`.
- **Score:** a knockout adds 1 to the bag count, and a burst adds 1 more (Task 16 wires the score).

- [ ] **Step 1: Write failing tests**
  - `test_ragdoll_pool_full`: 7 spawns. The 7th reuses the oldest slot. Active count stays 6.
  - `test_ragdoll_falls_and_rests`: a ragdoll spawned with zero launch at mid-air settles on the floor (every point y ≥ 0 − 1e-4) and deactivates after `RAGDOLL_REST_FRAMES` plus settling time, within 300 steps.
  - `test_ragdoll_constraints_hold`: after 100 steps of a hard launch (8, 4, 6), every stick length is within 15% of its rest length.
  - `test_wall_impact_damage`: a launch of (12, 0, 0) into the right wall reduces hp by roughly (12·0.9…−3). Assert `hp < RAGDOLL_HP` after the first contact, and that the burst is reported once hp ≤ 0 (repeat the launch, or set hp to 0.5 first).
  - `test_burst_reported_once`: a burst ragdoll appears in exactly one `RagdollStepOut`.
  - `test_bowling_damages_dummy`: a ragdoll launched at (0, 1, 8) toward a medium dummy 1.5 m ahead reduces the dummy's hp, sets `reaction_t > 0`, and `bowl_hits >= 1`. Hits from the same pair don't repeat within 10 frames.
  - `test_bowling_chain_knockout`: if the dummy's hp ≤ the bowl damage, it is knocked out and a second ragdoll becomes active.
  - `test_ragdoll_slams_open_door`: an open door at z=3, side +1, with a ragdoll launched into the right wall at z=3.5 becomes `DOOR_CLOSED_FULL` with `remaining` unchanged.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `ragdoll.c`, spawn on knockout in `game_update`, and `draw_ragdoll`. Draw each segment's part image rotated to the segment's projected angle, scaled by `px_per_m`, with the face on the head using the ko face. Draw ragdolls sorted with dummies.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: verlet ragdolls, wall damage, bowling, door slams"`

### Task 14: Burst particles and the burst-storm scenario

**Files:**
- Create: `src/core/particles.h/.c`
- Modify: `src/core/game.h/.c`, `src/core/draw_world.h/.c`, `src/core/bench.c`
- Create: `perf/scenarios/burst-storm/bench.json`, `perf/scenarios/burst-storm/input.json`, `perf/scenarios/wave/bench.json`, `perf/scenarios/wave/input.json`
- Test: `tests/test_particles.c`

**Interfaces:**
- Produces:

```c
typedef enum { PK_PUFF, PK_SCRAP, PK_TOOTH } ParticleKind;
typedef struct { bool active; ParticleKind kind; uint8_t variant; Vec3 p, v; float angle, spin; int life; bool settled; } Particle;
typedef struct { Particle p[256]; int next; } ParticlePool;
void particles_burst(ParticlePool*, Rng*, Vec3 at, Vec3 wall_normal); /* BURST_PUFFS puffs + BURST_SCRAPS scraps, sprayed in a hemisphere away from the wall; extra 12 puffs aimed at the floor below to form the pile */
void particles_teeth(ParticlePool*, Rng*, Vec3 at);                   /* 2 teeth up and out */
void particles_step(ParticlePool*);                                    /* gravity, floor settle (no bounce for puffs, one small bounce for scraps), life-- */
void draw_particles(RenderList*, const Camera*, const ParticlePool*);  /* far-to-near with the world sort */
```

  When the pool is full, new particles overwrite in ring order via `next` (oldest first). Settled puffs live the full `PARTICLE_LIFE_FRAMES`, so the stuffing pile stays visible for 5 seconds.

- Bench scenes:
  - **`BENCH_WAVE` (`distance` d):**
    - Places the player at d.
    - Opens the 4 nearest doors with `remaining = hallway_dummies_for_door(d)`.
    - Pre-spawns dummies up to the 16-dummy cap using sizes from `hallway_pick_size`.
  - **`BENCH_BURST_STORM`:** every 60 frames, spawns 6 small dummies and immediately knocks them out with launches of (±12, 3, 4) toward the walls, so that ragdolls and the particle pool are at maximum.

- [ ] **Step 1: Write failing tests**
  - `test_burst_counts`: one burst activates `BURST_PUFFS + BURST_SCRAPS + 12` particles.
  - `test_particles_pool_full`: 10 bursts leave active ≤ 256, `next` wraps, and no crash.
  - `test_particles_settle_on_floor`: after 120 steps, every puff has `p.y == 0` and `settled`.
  - `test_particles_expire`: after `PARTICLE_LIFE_FRAMES + 1` steps, none are active.
  - `test_burst_sprays_away_from_wall`: for a burst on the right wall (normal (−1,0,0)), every scrap's `v.x <= 0`.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** particles, burst and teeth spawning in `game_update`, drawing, and the two bench scenes.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Write the scenarios** and run `perf/run.sh wave burst-storm`.
  - `wave` uses `{"scene":"wave","distance":400,"seed":5}` with an input that cranks constantly at 20°/frame and holds left/right alternately.
  - `burst-storm` uses `{"scene":"burst-storm","seed":9}` with no input.
  - Expected: both pass with 0 over-budget frames on rev-a.
  - **If they fail:** read the hotspots as described in the perf skill. The likely cause is `drawRotatedBitmap`. Add a rotation cache in `pd_render.c`: for each image id, pre-render 32 rotation steps lazily at a quantized scale, keyed on `(image, angle_step, scale_bucket)` with 8 scale buckets, in a 96-entry LRU. Then re-run until the scenarios pass. Record before and after numbers in the commit message.
- [ ] **Step 6: Record baselines.** Write each scenario's `avg_work_ms × 1.3`, rounded up to 0.5 ms, into `perf/scenarios/<s>/max_avg_ms` for walk, fight, wave and burst-storm.
- [ ] **Step 7: Commit:** `git commit -m "burst particles, wave and burst-storm perf scenarios"`

---

## Milestone F: Full run and playtest build

### Task 15: Weapon offer between waves

**Files:**
- Modify: `src/core/game.h/.c`, `src/core/hud.h/.c` (create)
- Test: `tests/test_game_flow.c`

**Interfaces:**
- Produces:
  - `typedef enum { MODE_TITLE, MODE_PLAY, MODE_OFFER, MODE_GAME_OVER, MODE_SKINS, MODE_CREDITS } GameMode;` is a field on `Game`.
  - `Game` gains `WeaponId weapon; WeaponId offer; float pickup_z; /* -1 none */`.
  - **Pickup appears (Decided):** after every `WEAPON_OFFER_EVERY_DOORS` emptied doors, a pickup is placed on the floor 3 m ahead of the player. It offers one random weapon different from the current one.
  - **Offer screen:** walking into the pickup (within 0.5 m) enters `MODE_OFFER`. The world is paused and drawn dimmed with a `PAT_D50` overlay. A panel shows the current weapon on the left and the offered one on the right: icon, name, speed bars, power bars. A takes the new weapon; B keeps the current one. Either choice removes the pickup and returns to `MODE_PLAY`.
  - `void hud_draw_offer(RenderList*, WeaponId current, WeaponId offer);`
  - `void draw_pickup(RenderList*, const Camera*, float z, int icon_image);` draws a floor-level icon that bobs.

- [ ] **Step 1: Write failing tests**
  - `test_offer_after_4_doors`: setting `doors_emptied` from 3 to 4 during `game_update` sets `pickup_z = player_z + 3`, with `offer != weapon`.
  - `test_offer_take`: entering the pickup gives `MODE_OFFER`. Pressing A sets `weapon == offer` and returns to `MODE_PLAY` with `pickup_z == -1`.
  - `test_offer_keep`: pressing B leaves the weapon unchanged.
  - `test_offer_pauses_world`: in `MODE_OFFER`, 30 updates don't change the dummies' z values.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** the mode, offer logic, offer panel and pickup drawing.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "weapon offer between waves"`

### Task 16: HUD, score and save data

**Files:**
- Create: `src/core/save.h/.c`
- Modify: `src/core/hud.h/.c`, `src/core/game.h/.c`, `src/platform/playdate/main.c`
- Test: `tests/test_save.c`, `tests/test_hud.c`

**Interfaces:**
- Produces:
  - `Game` gains `int bags; float best_distance; int best_bags; int seen_new_best;`
  - `void hud_draw_play(RenderList*, const Game*);` matches mockup A:
    - hearts top left (5 slots, full/empty images)
    - distance top center as `"%dm"` from `(int)player_z`
    - bag icon + `"x%d"` top right
    - weapon panel bottom left: name, `SPD` bars, `PWR` bars
    - a swing-direction indicator, bottom center, with a left/right arrow when held
  - `rl_text` takes preformatted strings. Use a tiny `int_to_str(int, char*)` helper in `hud.c`, not `snprintf` with floats.
  - Save format: ASCII `key=value\n` lines in `save.txt`.

```c
typedef struct { int version; int best_bags; int best_distance_m; char sel[9][24]; /* per-slot pack id, "" = core */ } SaveData;
void save_defaults(SaveData*);
int  save_write(const SaveData*, char* buf, int cap);          /* returns bytes */
bool save_read(const char* buf, int len, SaveData* out);       /* never fails hard: unknown keys ignored, bad values keep defaults; returns false only if buf is NULL */
```

  Slot keys: `sel.dummy-small`, `sel.dummy-medium`, `sel.dummy-large`, `sel.weapon-chicken`, `sel.weapon-bat`, `sel.weapon-pan`, `sel.weapon-sledge`, `sel.stuffing`, `sel.sounds`.
  - **Score:** knockout `bags += 1`, and burst `bags += 1`.
  - **Death:** saves when the run ends with a new best.

- [ ] **Step 1: Write failing tests**
  - `test_save_roundtrip`: write then read gives equal structs.
  - `test_save_missing_and_empty`: `save_read` of `""` gives defaults.
  - `test_save_corrupt`:
    - `best_bags=abc\nbest_distance_m=-5\nzzz\n` gives defaults for both. Negative values are clamped to 0, and nothing crashes.
    - A `sel.` value longer than 23 chars is truncated.
    - An unknown version still loads the known keys.
  - `test_hud_text`: with `bags=12` and `player_z=48.7f`, the render list text contains `"x12"` and `"48m"`.
  - `test_score_burst_bonus`: a knockout plus a burst of the same dummy gives `bags == 2`.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `save.c` and `hud.c`. In `main.c`, load `save.txt` at init and write it when `Game` sets a `save_dirty` flag.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "HUD, scoring, save data"`

### Task 17: Title, game over and sound

**Files:**
- Create: `src/platform/playdate/pd_sound.c/.h`
- Modify: `src/core/game.h/.c`, `src/core/hud.h/.c`, `src/platform/playdate/main.c`
- Test: `tests/test_game_flow.c`

**Interfaces:**
- Produces:
  - `typedef enum { SND_SWING, SND_HIT_LIGHT, SND_HIT_HEAVY, SND_BURST, SND_GRUNT, SND_DOOR_SLAM, SND_STEP, SND_HURT, SND_COUNT } SoundId;` `Game` gains `uint8_t sounds[16]; int nsounds;`, which is cleared at the start of each `game_update`.
    - Push `SND_SWING` on `SWE_STARTED`.
    - Push `SND_HIT_HEAVY` on a hit with damage ≥ 1.4, otherwise `SND_HIT_LIGHT`.
    - Push `SND_GRUNT` on a knockout and `SND_BURST` on a burst.
    - Push `SND_DOOR_SLAM` on a player door slam and on a ragdoll door slam.
    - Push `SND_STEP` every 12 frames while walking.
    - Push `SND_HURT` when the player takes damage (`PEV_HURT`).
  - `void pd_sound_init(PlaydateAPI*, const char* sound_pack_dir); void pd_sound_play(PlaydateAPI*, SoundId);`
    - Uses one `SamplePlayer` per id.
    - The grunt plays variant `grunt-1..3`, rotating.
    - Missing files are logged and become silent.
  - **Title screen:**
    - The hallway is drawn with a dummy idling.
    - Shows "RAGDOLL DUMMIES", "Press A to start", "B: Skins", and the best bags and distance.
    - A starts a run with `game_init(seed)`.
    - B goes to `MODE_SKINS`, which is a stub until Task 21.
  - **Game over:** when the player dies, the screen shows "BAGGED: N", the distance, the bests, and "NEW BEST!" when applicable. A goes to the title.
  - Show the SDK crank indicator (`pd->system->drawCrankIndicator`, called in the platform after rendering, if available in SDK 3.1.2's `playdate_sys`) while `crank_docked` in `MODE_PLAY`. If the C API lacks it, draw a "Use the crank!" hint via the HUD.

- [ ] **Step 1: Write failing tests**
  - `test_title_to_play`: `MODE_TITLE` with A pressed gives `MODE_PLAY`, hearts 5 and bags 0.
  - `test_death_to_game_over`: forcing hearts to 0 gives `MODE_GAME_OVER` on the next update, and `save_dirty` is set when the bests improve.
  - `test_sound_events`: a swing start pushes `SND_SWING` exactly once, and the queue is empty on the next frame.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** the modes, screens, sound queue and `pd_sound.c`. The bench start skips the title.
- [ ] **Step 4: Run `make test`.** Expected: ok. Then run `perf/run.sh` for all scenarios. Expected: pass.
- [ ] **Step 5: Commit:** `git commit -m "title, game over, sound playback"`

### Task 18: Playtest build (checkpoint)

**Files:**
- Create: `docs/playtest.md`

- [ ] **Step 1: Run a full verification:** `make test && make && make check-doubles && tools/stage_pdsim.sh && perf/run.sh`. Expected: everything passes.
- [ ] **Step 2: Write `docs/playtest.md`** for the owner. It covers:
  - How to sideload `RagdollDummies.pdx` (via play.date/account/sideload, or USB disk mode → `Games/`).
  - The controls.
  - What to try: each swing direction, every weapon, wall bursts, bowling, door slams.
  - The feel questions that drive tuning:
    - Is the crank threshold too twitchy or too stiff?
    - Is the swing delay per weapon right?
    - Are the reactions readable?
    - Is the ragdoll too bouncy?
    - Is the difficulty curve right?
  - Where the tuning values live (`src/core/tuning.h`).
- [ ] **Step 3: Commit and push:** `git commit -m "docs: playtest guide"` and `git push`. Attach `RagdollDummies.pdx` as a CI artifact. Add an upload step to the `device-build` job that runs `make` (not just `device_bin`) and uploads `RagdollDummies.pdx`.
- [ ] **Step 4: Stop.** Send the owner the CI artifact link and `docs/playtest.md`. Wait for feedback. Apply tuning changes in `tuning.h` (and only where needed elsewhere) as separate commits before starting Milestone G.

---

## Milestone G: Skins

### Task 19: Pack manifest parsing, validation and slot resolution

**Files:**
- Create: `src/core/pack.h/.c`, `tests/fixtures/packs/` (good: `zombie/`, broken: `bad-json/`, `future-spec/`, `missing-name/`, `huge/`)
- Test: `tests/test_pack.c`

**Interfaces:**
- Consumes: `json_*` (Task 3), `SaveData.sel` (Task 16), `DummyRig`/`WeaponRig` (Task 7).
- Produces:

```c
typedef enum { SLOT_DUMMY_SMALL, SLOT_DUMMY_MEDIUM, SLOT_DUMMY_LARGE,
               SLOT_WEAPON_CHICKEN, SLOT_WEAPON_BAT, SLOT_WEAPON_PAN, SLOT_WEAPON_SLEDGE,
               SLOT_STUFFING, SLOT_SOUNDS, SLOT_COUNT } Slot;
const char* slot_key(Slot);            /* "dummy-small", ..., "weapon-bat", "stuffing", "sounds" */
typedef struct {
  char id[24], name[40], version[16], author[40], license[24], tip_url[96];
  uint16_t provides;                   /* bit per Slot */
  DummyRig rig[DS_COUNT]; bool has_rig[DS_COUNT];
  WeaponRig wrig[WEAPON_COUNT]; bool has_wrig[WEAPON_COUNT];
  char error[64];
} PackManifest;
#define PACK_MAX_JSON 16384
bool pack_parse(const char* json, int len, PackManifest* out); /* false + out->error on: len>PACK_MAX_JSON, invalid JSON, spec missing or >1, missing id/name/author/license, id not [a-z0-9-]{1,23}, provides empty, a provided dummy slot without anchors, height_px outside the size's range (small 60–110, medium 90–150, large 120–200) */
int  pack_resolve(const PackManifest* packs, int npacks, const char sel[SLOT_COUNT][24], Slot s); /* index of selected pack if loaded and provides s, else index of "core" (packs[0] by convention) */
```

- [ ] **Step 1: Write failing tests**
  - `test_pack_parses_good`: the `zombie` fixture parses. It has `provides` = small + medium + bat, the medium rig `height_px == 120`, and `tip_url` set.
  - `test_pack_rejects_bad_json`, `test_pack_rejects_future_spec` (`spec: 2`), `test_pack_rejects_missing_name`, and `test_pack_rejects_huge` (a 20 KB file) each return false, with a non-empty `error` that names the reason.
  - `test_pack_rejects_bad_id`: `"id":"Bad Id!"` returns false.
  - `test_pack_rejects_height_out_of_range`: a medium dummy with `height_px: 400` returns false.
  - `test_resolve_selected`: with the selection `dummy-small=zombie` and zombie loaded, `pack_resolve` gives zombie's index.
  - `test_resolve_falls_back_to_core`:
    - Selection `weapon-pan=zombie`, which zombie doesn't provide, gives index 0.
    - A selection naming a pack that isn't installed gives 0.
    - An empty selection gives 0.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** `pack.c`. The `core` manifest is parsed from `Source/packs/core/pack.json` at runtime like any other pack.
- [ ] **Step 4: Run `make test`.** Expected: ok.
- [ ] **Step 5: Commit:** `git commit -m "core: pack manifest parsing, validation, slot resolution"`

### Task 20: Loading community packs from the Data folder

**Files:**
- Modify: `src/platform/playdate/pd_assets.c/.h`, `src/platform/playdate/pd_sound.c`, `src/platform/playdate/main.c`
- Create: `perf/scenarios/skins/` (used in Task 21)

**Interfaces:**
- Consumes: `pack_parse`, `pack_resolve`, `SaveData.sel`.
- Produces:
  - `int pd_assets_discover(PlaydateAPI*, PackManifest packs[8]);` does the following:
    - Parses `packs/core/pack.json` into slot 0.
    - Then uses `pd->file->listfiles("packs", ...)`, which covers the Data folder and the bundle, for up to 7 more packs.
    - Skips `core`, unreadable or invalid packs, and duplicates. Each skip is logged with `pack.error`.
    - Returns the count.
  - `void pd_assets_apply_selection(PlaydateAPI*, const PackManifest*, int n, const char sel[SLOT_COUNT][24], LCDBitmap* images[IMG_COUNT], DummyRig rigs[DS_COUNT], WeaponRig wrigs[WEAPON_COUNT]);` does the following:
    - For each slot, it resolves the pack and loads each image from `packs/<id>/<slot-folder>/<file>`.
    - **Per image, on a missing file:** it loads the core image instead and logs it (Review Focus #2).
    - It frees previously loaded non-core bitmaps.
    - It copies the rig from the manifest (`has_rig`) or the core.
    - It sets the sound pack directory.
- The Data folder layout on device is `Data/com.jamesjnadeau.ragdolldummies/packs/<id>/...`, compiled with `pdc` (spec §8.4). For the simulator and pdsim, packs can be raw PNG in the data dir.
- **Optional hand-drawn reaction frames (spec §6.5):** a dummy slot may contain `reactions/<name>.png`. The names are `from-left`, `from-right`, `upward`, `downward`, `left-up`, `left-down`, `right-up` and `right-down`, matching `ReactionType` order from `RX_FROM_LEFT`.
  - Add `uint16_t reaction_mask;` to `DummyRig`, with bit = `ReactionType`.
  - Add the image range `IMG_REACT0` (+ size*8 + (rx−1)) to the enum in `rig.h`, before `IMG_UI_HEART_FULL`.
  - The platform sets the mask bit when it loads a frame.
  - `draw_dummy` draws that single frame (anchored at the base, scaled like the body) instead of the procedural parts while `reaction_t > 0` and the bit is set.

- [ ] **Step 1: Add a second test pack** to `tests/fixtures/packs/zombie/`. Generate it by running `make_core_pack.py --variant zombie --out tests/fixtures/packs/zombie --provides dummy-small,dummy-medium,weapon-bat`; add that flag to the script. It produces different faces (stitched mouth, X-patch eye) and leaves out `face-squish.png` on purpose.
- [ ] **Step 2: Write failing test** `test_draw_dummy_uses_custom_reaction_frame` in `tests/test_draw_world.c`:
  - With `rig.reaction_mask = 1<<RX_UPWARD` and a dummy with `reaction = RX_UPWARD, reaction_t = 5`, `draw_dummy` emits exactly one `RC_BITMAP`, with `image == IMG_REACT0 + size*8 + (RX_UPWARD-1)`.
  - With `reaction_t = 0`, it emits the normal parts.
  - Run `make test` and expect failure. Implement it, then run again and expect ok.
- [ ] **Step 3: Implement** discovery and apply-selection, and call them at init and whenever the Skins menu changes the selection.
- [ ] **Step 4: Verify with pdsim.**
  - Create `perf/scenarios/skins/data/packs/zombie` (a copy of the fixture) and `bench.json` `{"scene":"walk","seed":2}`, plus `save.txt` with `sel.dummy-small=zombie\nsel.dummy-medium=zombie\n`.
  - Extend `perf/run.sh` to copy everything in `perf/scenarios/<s>/data/` into the data dir.
  - Run `perf/run.sh skins` with frames enough to see a small or medium dummy.
  - Expected: the screenshot shows zombie faces, the log shows the fallback for `face-squish`, and there's no crash.
- [ ] **Step 5: Verify the broken-pack path.** Copy `tests/fixtures/packs/bad-json` into that data dir too, run again, and expect a log line naming the reason with the game running normally.
- [ ] **Step 6: Commit:** `git commit -m "load community packs from Data folder with per-image fallback"`

### Task 21: Skins menu with live preview, and credits

**Files:**
- Modify: `src/core/game.h/.c`, `src/core/hud.h/.c`, `src/core/bench.c`
- Test: `tests/test_skins_menu.c`

**Interfaces:**
- Produces:
  - `Game` gains `PackManifest* packs; int npacks; int menu_row; int preview_rx; bool selection_changed;` The platform owns the array.
  - `MODE_SKINS`, matching spec §8.5:
    - The left column lists the 9 slots (Small dummy, Medium dummy, Large dummy, Rubber Chicken, Bat, Frying Pan, Sledgehammer, Stuffing, Sounds), each with its current pack name.
    - Up and down move the row. Left and right cycle that slot through packs that provide it (core first).
    - The right side shows a live preview. The selected dummy size (or medium for other slots) cycles through reactions every 20 frames, with the selected weapon sweeping at it.
    - A sets `selection_changed` (the platform reloads assets and saves). B returns to the title. The crank in the menu does nothing.
  - `MODE_CREDITS` is reached from the title with Up+A: it lists the packs in use with author, license and `tip_url` as text (the tip URL is shown, not linked).
  - `BENCH_SKINS` opens `MODE_SKINS` directly.

- [ ] **Step 1: Write failing tests**
  - `test_menu_cycle_only_providers`: with core plus zombie (small, medium, bat) on the `Frying Pan` row, right stays on core. On `Small dummy`, right goes core → zombie → core.
  - `test_menu_apply_sets_flag`: A gives `selection_changed` true and `sel[SLOT_DUMMY_SMALL] == "zombie"`.
  - `test_menu_back`: B gives `MODE_TITLE`.
  - `test_preview_cycles_reactions`: after 20 updates, `preview_rx` changes.
- [ ] **Step 2: Run `make test`.** Expected: failures.
- [ ] **Step 3: Implement** the menu, preview, credits and `BENCH_SKINS`.
- [ ] **Step 4: Run `make test`.** Expected: ok. Then point `perf/scenarios/skins/bench.json` at `{"scene":"skins","seed":2}` with an input that presses down and right a few times. Run `perf/run.sh skins`. Expected: pass, and the screenshot shows the menu and preview.
- [ ] **Step 5: Commit:** `git commit -m "skins menu with live preview, credits"`

### Task 22: Pack validator tool and artist spec

**Files:**
- Create: `tools/validate_pack.c`, `docs/pack-spec.md`
- Modify: `Makefile` (target `validator` → `build/host/validate_pack`), `.github/workflows/ci.yml`
- Test: `tests/test_validator.sh`

**Interfaces:**
- Consumes: `pack_parse` (Task 19).
- Produces:
  - `build/host/validate_pack <pack-dir>` exits 0 and prints `OK <id>`.
  - Otherwise it exits 1 with one line per problem:
    - an invalid manifest (`pack.error`)
    - a provided slot missing a required file (the required list per slot is from spec §8.2; optional files are not required)
    - PNG dimensions (read from the IHDR chunk) over the per-part limit: body/head ≤ 2× the core medium size × (height_px/120), weapon `held` ≤ 80×240, icon exactly 32×32, puff/scrap ≤ 24×24
    - a non-PNG image file
    - a WAV that doesn't start with a `RIFF....WAVE` header
- `docs/pack-spec.md` is for artists. It covers:
  - the folder layout and every file per slot, with sizes
  - an anchor diagram, as an ASCII sketch of the medium dummy with neck, shoulders, face center and base
  - how the reactions are made from parts, and which optional frames and faces exist and their fallbacks
  - 1-bit rules
  - the sound list and format (WAV, mono, 22.05 or 44.1 kHz, 16-bit or ADPCM)
  - licensing (default CC BY 4.0)
  - how to test a pack locally with the Playdate Simulator Data folder or pdsim
  - that artists submit plain PNG/WAV, because the future skins repo's CI compiles them

- [ ] **Step 1: Write the failing test script.** It builds the validator and asserts:
  - `tests/fixtures/packs/zombie` → exit 0
  - `Source/packs/core` → exit 0
  - `bad-json` → exit 1
  - `future-spec` → exit 1
  - a fixture `missing-file/` (provides `weapon-bat` but has no `held.png`) → exit 1 and output containing `weapon-bat/held.png`
- [ ] **Step 2: Run `bash tests/test_validator.sh`.** Expected: failure.
- [ ] **Step 3: Implement** `validate_pack.c` (host-only; may use stdio) and add the `validator` target.
- [ ] **Step 4: Run `bash tests/test_validator.sh`.** Expected: all pass. Add it to the CI `host-tests` job.
- [ ] **Step 5: Write `docs/pack-spec.md`.** Also add a README section, "Make a skin", linking to it.
- [ ] **Step 6: Commit:** `git commit -m "pack validator and artist pack spec"`

---

## Milestone H: Release gates

### Task 23: Final gates, README and release build

**Files:**
- Modify: `README.md`, `.github/workflows/ci.yml`, `perf/scenarios/*/max_avg_ms`

- [ ] **Step 1: Run every scenario** (`perf/run.sh`) and update each `max_avg_ms` to the new `avg_work_ms × 1.3`, rounded up to 0.5 ms. Commit the changes with the measured numbers in the message.
- [ ] **Step 2: Make CI gating complete.**
  - `host-tests` (unit + validator) and `device-build` (`make`, `check-doubles`, upload the `.pdx`) are required.
  - `perf` runs all 5 scenarios with `--max-over-budget 0` plus `max_avg_ms`, and uploads the reports, traces and screenshots.
- [ ] **Step 3: Rewrite `README.md`.** Keep the original description. Add:
  - controls
  - how to build (`make`, `make test`)
  - how to run perf (`tools/pdsim.sh`, `perf/run.sh`)
  - the no-doubles rule
  - the project layout
  - "Make a skin" → `docs/pack-spec.md`
  - licenses: GPL-3.0 code and CC BY 4.0 core art
- [ ] **Step 4: Final verification:** `make test && bash tests/test_validator.sh && make && make check-doubles && tools/stage_pdsim.sh && perf/run.sh`. Expected: all pass. Quote the perf summary lines in the final report to the owner.
- [ ] **Step 5: Commit and push:** `git commit -m "release gates, README"` and `git push`.
