# Ragdoll Dummies: game design

Date: 2026-11-05
Status: approved by owner 2026-11-05
Mockups: https://claude.ai/artifact/6XxUjYLN4wmZ7db4HFTCA7 (first-person screen, dummy reactions)

Throughout this document, **Decided** marks what the project owner chose and
**Builder's call (proposed)** marks choices made during design that the owner
has not explicitly ruled on. Anything else is left to the builder.

---

## 1. Intent

An open-source Playdate game, shared with the Playdate community, whose fun
is hitting cartoon punching-bag dummies and watching them react. The game is
built from the start so that anyone can reskin it: community art packs are
first-class, players mix and match them, and a free community skin
marketplace (with optional tip links for artists) follows the game.

Success looks like: people enjoy playing it, and people make and share skins
for it.

A web version is wanted some day. It is not part of this project, but the
architecture must not make it hard.

## 2. Goals, non-goals, anti-goals

**Goals**
- A complete, endless first-person brawler on Playdate, written in C.
- Hits feel good: every hit direction has a distinct, funny reaction;
  knockouts send dummies flying as ragdolls; hard wall hits burst them into
  stuffing.
- A published, versioned skin pack spec. The default art is itself a pack.
- Players mix and match packs per piece (each dummy size, each weapon,
  stuffing, sounds).
- Holds 30 fps on device in the busiest late-game scenes, verified
  continuously with the playdate-performance-simulator.
- No `double` arithmetic anywhere in the shipped game.

**Non-goals (this project)**
- The skin marketplace website and in-game pack downloading (a follow-up
  project; this project only defines the pack format and loads packs from
  the Data folder).
- The web version (follow-up; this project keeps the core portable).
- Online leaderboards (Panic's scoreboards are Catalog-only).
- Skinning the hallway (it is always the same).
- Skins that change gameplay (stats stay in the game).
- Paid skins or accounts.

**Anti-goals (failure even if it technically works)**
- Hitting things is not fun or funny: reactions are stiff, samey, or hard to read.
- Frame drops during big moments (crowded waves, multiple ragdolls, bursts).
- A pack spec so loose that mixed packs look broken together, or so heavy
  that artists don't bother.
- Skin authors needing the Playdate SDK or a Playdate to contribute.

## 3. Constraints

- **Hardware:** 400×240 1-bit display, 168 MHz Cortex-M7 with a
  single-precision FPU, 16 MB RAM.
- **Language:** C (C99), Playdate SDK 3.1.2 (installed at `~/PlaydateSDK` in
  the dev environment), `arm-none-eabi-gcc` for device builds.
- **No doubles** (owner requirement). The M7's FPU is single precision only;
  any `double` operation becomes a slow software call. See §10.
- **License:** game code is GPL-3.0 (existing `LICENSE`). Art packs carry
  their own license (default CC BY 4.0).
- **Performance simulator compatibility** (see §11). The
  [playdate-performance-simulator](https://github.com/jamesjnadeau/playdate-performance-simulator)
  (pdsim) runs the device build `pdex.elf` in an emulator. It does not
  implement the SDK's JSON API, reads PNG images (not compiled `.pdi`) from
  the folder holding `pdex.elf`, does not simulate sound, and uses
  nearest-neighbour sampling for rotated/scaled drawing. The game must run
  correctly under it.
- **Future web build:** game logic must not depend on the Playdate API
  directly (see §5).

## 4. Approaches

Three ways to build it were considered:

1. **Own lightweight engine, game logic separated from the Playdate
   (chosen).** The hallway is a fixed straight corridor, so a small
   perspective projection draws it and sizes sprites by distance; no real 3D
   is needed. Dummies and weapons are 2D images drawn scaled/rotated. The
   ragdoll is a small hand-written Verlet simulation. All rules (combat,
   waves, physics, pack parsing) are plain C with no Playdate calls, behind a
   thin platform layer. Trade-off: we write the projection and physics
   ourselves (a few hundred lines each), but we control speed and feel, the
   core is unit-testable on a desktop, and a web platform layer can be added
   later.
2. **SDK's mini3d library for the hallway.** Real 3D geometry with 2D
   billboards. Rejected: the corridor is fixed and straight, so we would pay
   mini3d's cost and complexity for camera freedom we don't use, and still
   need the same sprite scaling code.
3. **C engine with Lua gameplay.** Rejected: two languages, Lua GC pauses
   risk stutter during ragdoll-heavy moments, and it moves away from "built
   in C". It is also unsupported by pdsim (Lua+C hybrids are not simulated).

Also considered: Chipmunk2D for physics. Rejected: our ragdolls need only a
simple point-and-stick model, and a hand-written one keeps the build,
determinism and licensing simple.

**Decided:** approach 1.

## 5. Architecture

### 5.1 Layers

```
src/core/        Pure C99. No pd_api.h, no libc I/O, no malloc during play.
                 Game rules, math, physics, hallway projection, pack manifest
                 parsing, render-list building. Deterministic given inputs + seed.
src/platform/playdate/
                 The only code that includes pd_api.h. Input → core input
                 struct; core render list → Playdate draw calls; asset and
                 pack loading via pd->file; sound playback; save files.
src/platform/web/   (future, not built now) Emscripten/WebAssembly + canvas.
tests/           Host unit tests for src/core (built with the host compiler).
tools/           Pack validator and any asset scripts.
perf/            pdsim input scripts, bench configs, CI thresholds.
packs/core/      The default art pack, in the same format as community packs.
```

The core never calls the platform. Each frame the platform:
1. fills a `GameInput` struct (buttons held/pressed, crank change in degrees
   as `float`, crank docked flag),
2. calls `game_update(&game, &input)` once (fixed 30 Hz step),
3. calls `game_render(&game, &render_list)`, which fills a flat array of draw
   commands (`DRAW_BITMAP` with image id, x, y, scale, angle, flip;
   `FILL_POLY` with a dither pattern id; `DRAW_TEXT`; `DRAW_LINE`),
4. executes the render list, and plays any sound events the core queued.

The render-list boundary is what makes host testing, pdsim debugging and a
future web build possible. Image ids are resolved by the platform from the
active pack selection (§8).

### 5.2 Memory and determinism

- All gameplay storage is fixed-size pools allocated at startup (dummies,
  ragdoll points, particles, render commands). No allocation inside
  `game_update`/`game_render`.
- One seeded PRNG (xorshift32) inside the game state. Same seed + same input
  sequence = same game. This makes pdsim runs and tests reproducible.
- **Builder's call (proposed):** starting pool sizes: 16 live dummies,
  6 simultaneous ragdolls, 256 stuffing/scrap particles, 512 render commands.
  Tune from pdsim measurements.

### 5.3 Builds

- **Device and Playdate Simulator:** the SDK's `common.mk` Makefile
  (`make`, `make device_bin`), which produces the `pdex.elf` pdsim expects.
- **Host tests:** a separate CMake (or Make) target that compiles
  `src/core` + `tests/` with the host compiler and runs them.
- **Decided:** game name "Ragdoll Dummies", bundle id
  `com.jamesjnadeau.ragdolldummies`.

## 6. The game

### 6.1 Camera and screen (Decided: first-person)

First-person view down the hallway, weapon resting at the bottom right,
as in mockup A.

**Decided:** no hands are drawn or animated. A swing is the weapon image
alone sweeping across the screen: it rotates around a pivot below the
screen edge, from the side the swing comes from to the other side (or
bottom to top for an upward swing, top to bottom for a downward one), with
speed lines. Weapon art is therefore a single image per weapon.

HUD (Builder's call, from the mockup): hearts top left,
distance in metres top centre, bag count top right, weapon name with speed
and power bars bottom left, swing-direction indicator bottom centre.
Taking damage: screen shake plus a one-frame inverted flash. Dummies never
come from behind (§6.7), so no rear warning is needed.

### 6.2 Controls

**Decided:**
- A: walk forward. B: walk back.
- D-pad left/right held: the swing comes from that side.
- D-pad up/down: aim the camera up or down, which also sets hit height.
- Crank: swings the weapon. Crank direction sets upward vs downward swing.
- Swing strength comes from crank speed.

**Builder's call (owner did not object):**
- No left/right held = a centre swing (straight uppercut or straight
  overhead chop, by crank direction).
- Crank clockwise (forward) = downward/overhead swing; counter-clockwise =
  upward swing.
- Aim has three levels (low, middle, high) (Decided). Up/down steps between them; the
  camera pitch eases to the level. Low aim is needed to hit small dummies'
  heads; high aim reaches large dummies' heads.
- A swing triggers when crank speed over the last few frames passes a
  threshold. Swing speed = peak crank speed during that window, clamped.
  These thresholds need tuning on a real device.
- Crank docked: show the SDK's crank indicator.

### 6.3 Weapons (Decided)

- Each weapon has a delay between cranking and the hit landing. Heavier
  weapons respond more slowly and do more damage.
- Weapons are picked up during the run, from a choice.

**Builder's call (proposed):**
- **Decided:** between waves, one weapon is offered. The player sees it
  next to their current weapon (speed and power bars for both) and either
  keeps what they have or takes the new one.
- Weapon stats live in game data (`src/core/weapons.c`), not in packs: name,
  delay (frames), power, reach (metres), arc width, ragdoll launch force.
- First-release roster of four, light to heavy: rubber chicken, baseball bat
  (starting weapon), frying pan, sledgehammer.
- Damage = power × speed factor (from crank speed) × zone multiplier
  (head 1.5, body 1.0). Exact numbers are tuning.

### 6.4 Dummies (Decided)

- Three sizes, cartoon punching bags with menacing faces and arms.
- They come out of doors and always path toward the player.
- They hurt the player.
- Small health pool; damage depends on swing speed and hit location.
- While alive: drawn reactions (not physics). Floppy (ragdoll) only once
  knocked out.

**Builder's call (proposed):**
- Behaviour: waddle toward the player, separate from each other slightly so
  they don't stack, wind up a readable punch when in reach, then hit.
  Bigger = slower wind-up, harder hit (small and medium 1 heart, large
  2 hearts).
- Starting health (tuning): small 2, medium 3, large 5, in bat-hit units.
- A hit staggers the dummy briefly and pushes it back.

### 6.5 Reactions while alive

Each reaction is a short procedural pose on the same body parts: lean, squash,
stretch, head offset/tilt, arm swing, face swap. No per-reaction frames are
required from artists; a pack may supply optional hand-drawn frames per
reaction, which replace the procedural pose.

Reactions (from the mockups, with owner feedback applied):
- **Hit from left:** leans right, arms fling back left, hurt face, impact
  star on the left.
- **Hit from right:** mirror.
- **Upward swing (Decided):** connects with the face; the neck stretches and
  the head snaps back toward the ceiling, eyes rolled up, teeth fly off.
- **Downward swing:** squashes flat, sees stars.
- Side + up/down combinations blend the two (e.g. left + upward = lean right
  and head snap).

### 6.6 Knockout, ragdoll and burst (Decided)

- When health is gone, the dummy is ragdolled away like a cartoon.
- When it hits a hard surface and its ragdoll health runs out, it bursts,
  its stuffing falls out, then it disappears.
- **Wall burst (Decided):** the dummy explodes into unrecognizable fabric
  scraps and a lot of stuffing that settles in a pile on the floor.

**Builder's call (proposed):**
- Ragdoll = 3D Verlet points in hallway space (x lateral, y up, z along the
  hallway): base, lower body, upper body, head, two elbows, two fists, with
  distance constraints and a few iterations per step. Initial velocity comes
  from the knockout hit's direction and weapon launch force.
- **Decided:** a flying ragdoll knocks into live dummies in its path and
  damages them (more damage at higher speed), and can knock them back or
  out, so one big hit can bowl through a crowd.
- **Decided:** a flying ragdoll that hits an open door slams it shut. The
  dummies still inside stay in; the door counts as a door that still has
  dummies, so it bursts open again as the player tries to pass (§6.7). A
  dummy that is in the doorway when it slams takes damage.
- Collides with floor, ceiling and both walls. Impact speed above a threshold
  reduces ragdoll health; at zero it bursts. Big hits can burst on the first
  wall.
- Burst spawns scrap and stuffing particles (simple ballistic, settle on the
  floor, fade after a few seconds). The dummy is removed.
- The ragdoll is drawn by projecting each segment and drawing its part image
  scaled and rotated along the segment.

### 6.7 A run (Decided)

- Walk as far down the hallway as you can. When you die, you see your bag
  count.
- The deeper you go, the more enemies you must defeat at once, and the more
  doors and enemies spawn.

**Decided:** dummies block the way. They are solid: the player cannot walk
through or past a dummy, so moving forward means knocking out the ones in
front. Distance counts forward progress only. Dummies that reach the player
hurt them.

**Decided:** doors are closed behind you. Dummies can't come out of a door
you've passed, and you can't get past a door while a dummy can still come out
of it. As you walk past a door you close it. If dummies are still inside,
the door bursts back open: the slam throws you back (knockback) and the next
dummy comes out. So every door must be emptied before you pass it, and all
fighting happens in front of you.

**Builder's call (proposed):**
- Bag count = dummies knocked out, plus a small bonus per burst.
- Each door holds a number of dummies that come out one after another as
  the player approaches. Dummies per door, doors open at once, and the mix
  of sizes all increase with distance.
- Door-slam knockback pushes the player back about a metre and costs no
  health; its purpose is to say "not yet", not to punish.
- Player has 5 hearts; no healing in the first release.
- Best bag count and best distance are saved on the device.
- Simple title screen, game-over screen with bag count and bests, Skins menu
  (§8.5).

### 6.8 Hallway

Fixed look, not skinnable. A straight corridor with doors on both walls,
floorboards, ceiling lights and darkness at the far end, drawn with dither
patterns as in the mockup. Doors are generated procedurally as the player
advances.

## 7. Rendering and projection

- World units are metres. Hallway half-width 1.6 m, height 2.7 m, camera
  height about 1.35 m. Screen x = cx + X·f/z, screen y = cy − (Y − camY)·f/z,
  with f ≈ 150 and cy shifted by aim pitch.
- Draw order: hallway → far-to-near sorted dummies, ragdolls and particles →
  weapon → HUD.
- Sprite size is set by z; dummies are drawn from parts.
- Rotated and scaled bitmap drawing is the expected performance hot spot.
  The builder should measure with pdsim first and, only if needed, add
  pre-scaled or pre-rotated caches (for example a few scale buckets and
  16–32 rotation steps per part).

## 8. Skin pack spec (v1)

### 8.1 What packs can contain (Decided)

Dummies (each size separately), weapons, stuffing and sounds. Players mix
and match across packs. The default art ships as a normal pack (`core`).

### 8.2 Pack layout

```
<pack-id>/
  pack.json
  dummy-small/   body.png head.png arm-upper.png arm-lower.png fist.png base.png
                 face-mean.png face-hurt.png face-dazed.png face-ko.png
                 [face-whack.png face-squish.png] [reactions/<name>.png ...]
  dummy-medium/  (same)
  dummy-large/   (same)
  weapon-<id>/   held.png (the weapon only, no hands) icon.png
  stuffing/      puff-1.png [puff-2..4.png] scrap-1.png [scrap-2..4.png]
  sounds/        swing.wav hit-light.wav hit-heavy.wav burst.wav
                 grunt-1.wav [grunt-2..3.wav] door-slam.wav ...
```

A pack includes only the folders for the pieces it covers. Optional files
fall back (for example `face-whack` → `face-hurt`, missing grunt variants →
`grunt-1`).

### 8.3 pack.json

```json
{
  "spec": 1,
  "id": "zombie-bags",
  "name": "Zombie Bags",
  "version": "1.0.0",
  "author": "Name",
  "license": "CC-BY-4.0",
  "tip_url": "https://ko-fi.com/name",
  "provides": ["dummy-small", "dummy-medium", "weapon-bat", "sounds"],
  "anchors": {
    "dummy-medium": {
      "height_px": 120,
      "neck": [0, -78], "shoulder_l": [-20, -64], "shoulder_r": [20, -64],
      "face_center": [0, -94]
    },
    "weapon-bat": { "grip": [12, 140], "tip": [96, 8] }
  }
}
```

- `spec` lets the game reject packs written for a newer format.
- Anchors place parts consistently so mixed packs fit together; the spec
  fixes a reference size per dummy size (`height_px` must be within a set
  range) and the image size limits.
- Images: PNG, 1-bit (black, white, transparent). Sounds: WAV (exact
  formats per the SDK's supported sample formats).
- **Builder's call (proposed):** the game parses `pack.json` with its own
  small JSON-subset parser in `src/core`, not the SDK's JSON API. Reasons:
  pdsim does not implement the SDK JSON API, the future web build needs it,
  and it can be unit-tested on the host.

### 8.4 Where packs live

- `packs/core/` in this repo is compiled into the game.
- Community packs go in the game's Data folder, `Data/<bundle-id>/packs/<id>/`,
  copied over USB. The Playdate file API reads the Data folder first, then
  the game bundle (pdsim behaves the same way with `--data-dir`).
- On device, images must be compiled to the Playdate format. Artists submit
  plain PNG/WAV; an automated build (in the future skins repo) runs `pdc`
  and publishes ready-to-copy packs, so artists never need the SDK.
- `tools/` includes a pack validator (sizes, required files, anchors,
  manifest) that artists and CI can run.

### 8.5 Mix and match (Decided)

A Skins menu lists installed packs. For each piece (small, medium, large
dummy; each weapon; stuffing; sounds) the player picks which pack supplies
it, with a live preview of a dummy reacting. The selection is saved. Any
piece not provided by the chosen pack falls back to `core`.

### 8.6 Marketplace (follow-up project, Decided: free now, tips later)

A separate public skins repo indexes packs. Artists submit packs by pull
request; CI validates and compiles them; a static site lists packs with
previews, credits and optional tip links. In-game downloading over Wi-Fi
(SDK HTTP API) comes after. This project only needs the manifest fields
(`author`, `license`, `tip_url`) and an in-game credits view.

## 9. Sound

Sound events (swing, hit light/heavy, burst, grunts, footsteps) are queued
by the core and played by the platform layer from the selected sound pack.
Sound is not simulated by pdsim, which is fine.

## 10. No doubles (owner requirement)

- `float` everywhere. All literals take the `f` suffix; use `sinf`, `cosf`,
  `sqrtf`, `fabsf`, and so on. Prefer lookup tables for trig in hot paths.
- Compile with `-Wdouble-promotion -Wfloat-conversion -fsingle-precision-constant -Werror`
  for both device and host builds.
- No `printf`-style `%f` (varargs promote to double). Format numbers as
  integers or fixed-point.
- **CI guard:** after the device build, fail if `pdex.elf` references
  double-precision helper symbols (`__aeabi_d*`, `__adddf3`, `__muldf3`,
  and similar), checked with `arm-none-eabi-nm`.

## 11. Performance simulator integration

The [playdate-performance-simulator](https://github.com/jamesjnadeau/playdate-performance-simulator)
(pdsim) runs `pdex.elf` against a virtual device clock and reports frame
times, hotspots by C symbol, memory, and over-budget frames.
Runs are deterministic.

- **Pinned tool:** CI builds `pdsim-cli` from a pinned commit of that repo
  (currently `6af8292`) and caches it.
- **Bench mode:** if a `bench.json` file exists in the Data folder (pdsim's
  `--data-dir`), the game skips the title screen and starts a scripted
  scene with a fixed seed. For example: `{"scene": "wave", "distance": 400,
  "seed": 1}` or `{"scene": "burst-storm"}`. This lets pdsim reach the
  heaviest scenes without long input scripts.
- **Scenarios** in `perf/`, each with a `bench.json` and an input script:
  1. early game walk and fight,
  2. late wave: maximum live dummies, all three sizes,
  3. burst storm: maximum simultaneous ragdolls and bursts and a full
     particle pool,
  4. Skins menu with preview.
- **Assets for pdsim:** pdsim reads PNG from the folder holding `pdex.elf`,
  so the perf job stages `pdex.elf` next to the source PNG assets and the
  `core` pack.
- **CI gates:** `--max-over-budget 0` on every scenario at 30 fps, plus a
  `--max-avg-ms` limit set from a measured baseline with headroom. Reports,
  traces and screenshots are kept as build artifacts.
- **Calibration (Decided: the owner has a Rev A Playdate):** all runs use
  the simulator's device-fitted `calibration/rev-a.json`, so the gates are
  hard failures, not warnings.
- **Agent skill:** copy the simulator's
  `.claude/skills/playdate-perf-test` skill into this repo so agents
  working here profile the game the same way.
- **Development habit:** after any change to rendering, physics or spawning,
  run the relevant scenario and compare before and after.

## 12. Testing

- **Host unit tests** for `src/core`, written test-first: projection, PRNG,
  crank-to-swing detection, damage and hit zones, dummy state machine, wave
  scaling, Verlet constraints and collisions, burst trigger, pack manifest
  parsing and fallback resolution, save data round-trip.
- **pdsim runs** as integration tests: each scenario must load, reach the
  expected scene (check the screenshot) and stay within budget.
- **Pack validator** tests against good and broken sample packs.
- Feel tuning (crank thresholds, reaction timing, ragdoll bounciness) needs
  play on real hardware; numbers here are starting points.
- **Owner playtest (Decided):** the owner playtests on their Rev A Playdate
  once there is a working version: a playable run with walking, swinging,
  reactions, knockouts and bursts, built as a `.pdx` they can sideload.
  Their feedback drives a tuning pass before the rest of the first release
  is finished. Tuning values should live in one place (a single tuning
  header or table) so that pass is quick.

## 13. Web version (future, not built now)

Kept possible by §5: the core has no Playdate dependency and talks through
`GameInput`, the render list and sound events. A later `src/platform/web/`
can compile the core with Emscripten, draw the render list to a 400×240
canvas, map keyboard to buttons and mouse wheel or drag to the crank, and
load packs from PNG/WAV directly. Packs stay as plain PNG/WAV in source
form, which the web can use directly.

## 14. First release scope

In: the full game loop (title, run, game over, bests), first-person
hallway, three dummy sizes, four weapons with the pickup choice, all
reactions, ragdolls and bursts, the `core` pack, loading community packs
from the Data folder, the Skins menu with mix-and-match, credits view, the
pack spec document and validator, host tests, pdsim CI gates, and the
no-doubles guard.

Out: marketplace site, in-game downloads, web build, leaderboards,
healing/power-ups, hallway skins.

## 15. Left to the builder

- Exact tuning numbers (damage, health, wave curve, crank thresholds,
  Verlet stiffness, bounce, impact thresholds).
- Internal module boundaries inside `src/core` and the render-command set.
- Host test framework (a small vendored single-header framework is fine).
- Art for the `core` pack within the spec (placeholder art is fine first).
- Whether rotation/scale caches are needed, decided from pdsim measurements.
