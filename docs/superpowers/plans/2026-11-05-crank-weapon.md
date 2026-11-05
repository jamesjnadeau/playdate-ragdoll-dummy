# Crank-Driven Weapon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the canned, threshold-triggered swing with a weapon that follows the crank through a weighted spring, sweeps across the dummies on screen, and either goes through or bounces off depending on momentum.

**Architecture:**
- A new `weapon_motion` module owns the weapon's state (angle, velocity, attached or loose, pivot shift) and steps it from the crank each frame.
- A new `contact` module sweeps the weapon's on-screen grip→tip segment from last frame's angle to this one against each in-reach dummy's projected head and body boxes. Those boxes come from a new `dummy_hit_boxes` in `pose.c`. The module applies damage, then penetration or bounce.
- `game.c` wires the two together. `swing.[ch]` and the old `combat_resolve` are deleted.

**Tech Stack:** C99, Playdate C SDK 3.1.2, host tests via `make test` (`tests/test.h` macros: `CHECK`, `CHECK_EQ_INT`, `CHECK_NEAR`, `RUN`).

**Spec:** `docs/superpowers/specs/2026-11-05-crank-weapon-design.md` (approved). Read it before starting any task.

**Branch:** `crank-weapon`. Commit after each task. Do not push.

## Global Constraints

- Floats only, with an `f` suffix on every literal. `-Wdouble-promotion -Wfloat-conversion -fsingle-precision-constant -Werror` must stay clean, and `make check-doubles` must pass.
- `src/core` stays pure C with no SDK includes. Only `src/platform/playdate/*` calls the SDK.
- Angles are in degrees. 0 means the weapon points straight up, and a positive angle turns clockwise on screen (`x = px + r·sin a`, `y = py − r·cos a`), as `polar()` in `draw_world.c` does today.
- Pivot: `WEAPON_PIVOT_X 200.0f`, `WEAPON_PIVOT_Y 376.0f`. The grip sits `WEAPON_SWEEP_RADIUS` (60 px) from the pivot.
- No frame may pass where the crank moves (undocked, attached) and the weapon's `vel` stays 0.
- Any newly added module (`.c` file) is picked up automatically by the Makefile's `src/core/*.c` and `tests/*.c` globs. Register each new test suite in `tests/main.c`.
- Run `make test` and `make check-doubles` before every commit. Both must pass.

## Review Focus

1. **Long cranking sessions.** The weapon and target angles are unwrapped and grow without limit. After about 10⁵° a float loses sub-degree precision. Rebase both angles by whole turns once they pass ±3600°. Test: 20,000 frames at 40°/frame, and the chicken still tracks the crank within 0.5° (Task 2).
2. **Docking or undocking mid-bounce.** Docking resets the weapon to upright. Undocking teleports the weapon to the crank angle without colliding (Task 2 tests the motion; Task 4 tests that a non-swept frame never hits).
3. **Reconnect across the ±180° wrap.** A wrapped difference that jumps from +179° to −179° must not read as the crank crossing the weapon. Turning a full turn forward to meet a loose weapon must still reconnect (Task 2).
4. **More contacts than `max_out`.** When more dummies are struck than `HitResult` slots, those beyond `max_out` must take no damage and change no state (Task 4).
5. **Pivot shift or aim change into a still weapon.** Sliding the pivot (left/right) or changing the aim (up/down) so that a resting weapon overlaps a dummy must not hit it. A later real swing that starts inside the dummy must not count as entering it either (Task 4).

---

### Task 1: Weapon stats

Add the physics fields to the weapon table. Replace the speed bar with a weight bar. `delay` and `arc` are still read by `swing.c`, so they stay until Task 5 removes them.

**Files:**
- Modify: `src/core/weapons.h`, `src/core/weapons.c`, `src/core/hud.c:36-38,149-151`
- Test: `tests/test_swing.c` (the table checks at the top of `suite_swing`)

**Interfaces:**
- Produces:
  - `WeaponDef` gains `float mass, spring, damp;`.
  - `int speed_bars` is renamed to `int weight_bars`.
  - Field order after this task: `name, folder, delay, power, reach, arc, launch, mass, spring, damp, weight_bars, power_bars`.

Values (spring and damp were found by simulation to meet the spec's catch-up targets. See Task 2):

| Weapon | mass | spring | damp | weight_bars |
|---|---|---|---|---|
| Rubber Chicken | 0.4f | 0.4f | 1.0f | 1 |
| Bat | 1.0f | 0.5f | 0.77f | 2 |
| Frying Pan | 1.4f | 0.5f | 0.67f | 3 |
| Sledgehammer | 2.6f | 0.5f | 0.49f | 6 |

`power`, `reach` and `launch` are unchanged (0.6/1.3/3.0, 1.0/1.6/5.0, 1.4/1.4/6.0, 2.4/1.8/9.0).

- [ ] **Step 1: Write the failing test.** Add the following to `tests/test_swing.c`:
  - Check `weapon_def(W_CHICKEN)->mass` is near 0.4f, `W_BAT` near 1.0f, `W_PAN` near 1.4f and `W_SLEDGE` near 2.6f.
  - Check masses strictly increase in that order.
  - Check `weight_bars` is 1, 2, 3, 6.
  - Check every `damp` is in (0, 1] and every `spring > 0`.
- [ ] **Step 2: Run `make test` and confirm it fails.** Expected: a compile error, because there is no member `mass`.
- [ ] **Step 3: Add the fields and values.**
  - In `hud.c`, change the offer-column label `"Speed"` to `"Weight"` and the play-HUD label `"SPD"` to `"WGT"`. Both read `w->weight_bars`.
- [ ] **Step 4: Run `make test` and confirm it passes (all suites).**
- [ ] **Step 5: Commit.** `git commit -am "weapons: mass, spring, damp and weight bars"`

---

### Task 2: Weapon motion module and crank angle input

**Files:**
- Create: `src/core/weapon_motion.h`, `src/core/weapon_motion.c`, `tests/test_weapon_motion.c`
- Modify: `src/core/input.h` (add a field), `src/platform/playdate/pd_input.c` (fill it), `src/core/tuning.h` (constants), `tests/main.c` (register `suite_weapon_motion`)

**Interfaces:**
- Consumes: `WeaponDef.mass/spring/damp` (Task 1), `GameInput`.
- Produces (`weapon_motion.h`):

```c
typedef struct {
    float angle, prev_angle;   /* degrees, unwrapped; prev_angle is the start of this frame's move */
    float vel;                 /* degrees per frame, signed */
    float target;              /* crank target angle, unwrapped */
    float shift;               /* pivot x offset from WEAPON_PIVOT_X, px */
    float prev_diff;           /* wrapped target - angle at the end of the last loose frame */
    bool live;                 /* false while docked / before the first undocked frame */
    bool attached;
    bool swept;                /* this frame's prev_angle -> angle move was a spring step and may hit */
    uint16_t inside;           /* bit i: the weapon overlaps dummy i (owned by contact.c) */
} Weapon;                      /* all-zero is a valid docked, centred weapon */

float weapon_wrap_deg(float a);                 /* to (-180, 180] */
float weapon_pivot_x(const Weapon* wp);         /* WEAPON_PIVOT_X + shift */
void  weapon_update(Weapon* wp, const GameInput* in, const WeaponDef* w);
void  weapon_detach(Weapon* wp, float angle, float vel);  /* used by contact.c on a bounce */
```

- `GameInput` gains `float crank_angle;` (degrees, 0 = crank arm up, as `getCrankAngle()` returns).
- `pd_input.c` sets it with `in->crank_angle = pd->system->getCrankAngle();`.

**Tuning constants** (add to `tuning.h` in a "Weapon motion" block):

| Constant | Value |
|---|---|
| `CRANK_ZERO_DEG` | 0.0f |
| `WEAPON_MAX_VEL` | 40.0f |
| `WEAPON_BOUNCE_DAMP` | 0.85f |
| `WEAPON_STOP_VEL` | 0.5f |
| `WEAPON_RECONNECT_DEG` | 3.0f |
| `WEAPON_SHIFT_PX` | 100.0f |
| `WEAPON_SHIFT_PX_PER_FRAME` | 10.0f |
| `WEAPON_REBASE_DEG` | 3600.0f |

**`weapon_update` order (the algorithm the tests pin):**
1. **Shift.** Ease `shift` toward −`WEAPON_SHIFT_PX` (LEFT held), +`WEAPON_SHIFT_PX` (RIGHT held) or 0 (neither, or both held), by at most `WEAPON_SHIFT_PX_PER_FRAME` per frame.
2. Set `prev_angle = angle` and `swept = false`.
3. **Docked** (`in->crank_docked`): set `live = attached = false`, `angle = prev_angle = target = vel = 0`, and return.
4. **First undocked frame** (`!live`): set `live = attached = true`, `target = angle = prev_angle = crank_angle − CRANK_ZERO_DEG` and `vel = 0`, then return. This is a teleport and is never swept.
5. `target += in->crank_delta`.
6. **Attached:**
   - `err = target − angle`
   - `vel += err·spring/mass − vel·damp`
   - clamp `vel` to ±`WEAPON_MAX_VEL`
   - `angle += vel`
   - `swept = true`
7. **Loose:**
   - `angle += vel`, then `vel *= WEAPON_BOUNCE_DAMP`. If |vel| < `WEAPON_STOP_VEL`, set `vel = 0`.
   - Compute `diff = weapon_wrap_deg(target − angle)`.
   - Reconnect if |diff| < `WEAPON_RECONNECT_DEG`, or if `diff` and `prev_diff` have opposite signs and both have magnitude < 90° (a real crossing, not the ±180° seam).
   - On reconnect: set `target = angle + diff`, then `angle = target`, `vel = 0`, `attached = true`. `swept` stays false.
   - Otherwise set `prev_diff = diff`.
8. **Rebase:** if |angle| > `WEAPON_REBASE_DEG`, subtract the same whole number of turns (`360·k`) from `angle`, `prev_angle` and `target`.

`weapon_detach` sets `angle`, `vel`, `attached = false` and `prev_diff = weapon_wrap_deg(target − angle)`.

- [ ] **Step 1: Write the failing tests** in `tests/test_weapon_motion.c`.

  Use a helper `crank(Weapon*, const WeaponDef*, float delta, int frames)` that steps undocked frames with `crank_delta = delta`. The catch-up metric used below:
  - Turn at 20°/frame for 30 frames, then 0°/frame.
  - Count the frames until both |target − angle| < 2° and |vel| < 1, staying that way through frame 80.

  Each test, with its expected result:

  | Test | Expected result |
  |---|---|
  | `test_zero_is_docked_upright` | A zeroed `Weapon` updated with a docked input has `angle == 0`, `!attached`, `!swept`. |
  | `test_undock_teleports` | Zeroed weapon, undocked input with `crank_angle = 90`, `crank_delta = 90` → `angle == 90`, `prev_angle == 90`, `attached`, `!swept`, `vel == 0`. |
  | `test_moves_same_frame` (one per weapon) | After the undock frame, a single frame with `crank_delta = 5` gives `vel != 0` and `swept`. |
  | `test_catch_up_targets` | The catch-up metric is ≤2 frames for the chicken, ≤4 for the bat, ≤6 for the pan and ≤10 for the sledge. |
  | `test_heavier_lags_more` | Steady lag (`target − angle` after 30 frames at 20°/frame) is < 0.5° for the chicken, and strictly increases chicken < bat < pan < sledge. |
  | `test_sledge_overshoots_slightly` | After the stop, the sledge's maximum `angle − target` is > 0 and < 8°. |
  | `test_vel_clamped` | One frame with `crank_delta = 720` gives \|vel\| ≤ `WEAPON_MAX_VEL`. |
  | `test_loose_slows_and_stops` | `weapon_detach(wp, 30, -16)` with the crank still → `vel` follows ×0.85 per frame and reaches exactly 0 within 30 frames. `attached` stays false while the crank doesn't move. |
  | `test_reconnect_turning_back` | Detach at angle 30 with vel −10 while the target is 40. Turning the crank back (−5/frame) reattaches with `angle == target` (within 0.001) on the first frame the target passes the weapon. |
  | `test_reconnect_full_turn_forward` | Same detach. Turning the crank forward +10/frame reattaches after about one turn. At the reattach frame, `weapon_wrap_deg(target − angle)` is near 0 and \|target − angle\| < 1. |
  | `test_no_false_reconnect_at_seam` | Weapon loose at angle 0, vel 0, target 170, crank +5/frame for 3 frames (diff passes 180 → −175) → still loose. |
  | `test_pivot_shift_eases` | Holding LEFT for 5 frames gives `shift == -50`. Holding it for 20 frames gives `-WEAPON_SHIFT_PX`. Releasing for 20 frames gives 0. `weapon_pivot_x` = 200 + shift. |
  | `test_dock_mid_bounce_resets` | Loose and moving, then a docked frame → `angle == 0`, `vel == 0`, `!live`. The next undocked frame teleports and is `!swept`. |
  | `test_long_session_stays_precise` | Chicken, 20,000 frames at 40°/frame → \|weapon_wrap_deg(target − angle)\| < 0.5 and \|angle\| ≤ `WEAPON_REBASE_DEG` + 360. |

- [ ] **Step 2: Run `make test` and confirm it fails.** Expected: a compile error, because `weapon_motion.h` is not found.
- [ ] **Step 3: Implement `weapon_motion.c`** following the order above. Add `crank_angle` to `GameInput` and set it in `pd_input.c`.
- [ ] **Step 4: Run `make test` and confirm it passes.** If a catch-up test misses, tune only that weapon's `spring`/`damp` in `weapons.c` and update Task 1's test if it pins the value.
- [ ] **Step 5: Commit.** `git add -A && git commit -m "weapon_motion: spring-coupled crank weapon, bounce and reconnect"`

---

### Task 3: Projected dummy hit boxes

**Files:**
- Modify: `src/core/pose.h`, `src/core/pose.c` (next to `draw_dummy`, reusing `DrawCtx`/`to_screen`), `src/core/tuning.h`
- Test: `tests/test_pose.c`

**Interfaces:**
- Produces (`pose.h`):

```c
typedef struct { float x0, y0, x1, y1; } ScreenBox;   /* x0 <= x1, y0 <= y1 */
/* Screen boxes a weapon can strike for a dummy drawn with this pose. False when draw_dummy would draw nothing
   (behind the near plane, past FAR_Z, or off the sides). */
bool dummy_hit_boxes(const Camera* cam, const Dummy* d, const DummyRig* rig, const DummyPose* pose,
                     ScreenBox* head, ScreenBox* body);
```

**Box layout:**
- The boxes use rig proportions only, never `part_w/h`, because those are 0 in host tests. That keeps the host and the device identical.
- Each corner is taken through the same base projection, scale, pose scale and lean as `draw_dummy` (`to_screen`), and the axis-aligned bound of the four transformed corners is returned.
- **Head:** x from `neck.x + head_dx ± HIT_HEAD_HALF_W_FRAC·height_px`, y from `neck.y + head_dy − HIT_HEAD_H_FRAC·height_px` to `neck.y + head_dy`.
- **Body** (arms included): x from `shoulder_l.x − HIT_ARM_PAD_FRAC·height_px` to `shoulder_r.x + HIT_ARM_PAD_FRAC·height_px`, y from `neck.y` to 0.

| Constant | Value |
|---|---|
| `HIT_HEAD_HALF_W_FRAC` | 0.17f |
| `HIT_HEAD_H_FRAC` | 0.34f |
| `HIT_ARM_PAD_FRAC` | 0.12f |

The early returns copy `draw_dummy`'s three checks exactly, so a box exists if and only if the dummy is drawn.

- [ ] **Step 1: Write the failing tests** in `tests/test_pose.c`. Use a camera at `player_z 0` with the aim at 1, a medium `DST_WALK` dummy at x 0, z 2, `rig_default` rigs and a pose from `pose_for_state`.

  | Test | Expected result |
  |---|---|
  | `test_hit_boxes_contain_anchor_points` | The projection used by `spawn_impacts` (face centre and body centre at height `−anchor.y/height_px·dummy_height`) puts the face point inside `head` and the body point inside `body`. |
  | `test_head_above_body` | `head.y1 <= body.y0 + 1`. Both boxes are centred within 2 px of the projected `x`. |
  | `test_boxes_scale_with_size` | At the same position, a large dummy's body box is taller than a small one's. A dummy at z 6 has a smaller box than one at z 2. |
  | `test_boxes_none_when_not_drawn` | z = `player_z + 0.1f` (inside the near plane), or z beyond `FAR_Z` → returns false. |
  | `test_boxes_follow_aim` | Changing `cam.aim` (after `camera_update` settles) moves both boxes vertically. |

- [ ] **Step 2: Run `make test` and confirm it fails.** Expected: a compile error, because `ScreenBox` is undefined.
- [ ] **Step 3: Implement `dummy_hit_boxes`.**
- [ ] **Step 4: Run `make test` and confirm it passes.**
- [ ] **Step 5: Commit.** `git commit -am "pose: projected head and body hit boxes"`

---

### Task 4: Swept contact resolver

This adds a new module. The old `combat_resolve` stays until Task 5, so the build stays green.

**Files:**
- Create: `src/core/contact.h`, `src/core/contact.c`, `tests/test_contact.c`
- Modify: `src/core/combat.h` (add `float sx, sy;` to `HitResult`, the screen contact point), `src/core/tuning.h`, `tests/main.c` (register `suite_contact`)

**Interfaces:**
- Consumes: `Weapon`, `weapon_pivot_x`, `weapon_detach`, `weapon_wrap_deg` (Task 2); `dummy_hit_boxes`, `ScreenBox`, `pose_for_state`, `pose_for_reaction` (Task 3); `WeaponDef.mass/power/launch/reach`; `WeaponRig.grip/tip`; `HitResult`, `ReactionType`.
- Produces (`contact.h`):

```c
/* Grip and tip on screen for the weapon at angle_deg about the current pivot. */
void  weapon_segment(const Weapon* wp, const WeaponRig* wr, float angle_deg,
                     float* gx, float* gy, float* tx, float* ty);
float weapon_tip_radius(const WeaponRig* wr);            /* WEAPON_SWEEP_RADIUS + |tip - grip| */
float contact_momentum(const WeaponDef* w, float vel_deg, float r_px);  /* mass * |vel|*pi/180 * r */
float dummy_resist(DummySize s);
ReactionType contact_reaction(float angle_deg, float vel);
/* Sweeps wp from prev_angle to angle against every in-reach dummy, applies hits, and may detach wp (bounce).
   Always refreshes wp->inside. Returns the number of HitResults written (<= max_out). */
int contact_sweep(Weapon* wp, const WeaponDef* w, const WeaponRig* wr, const Camera* cam,
                  DummyPool* pool, const DummyRig rigs[DS_COUNT], int frame,
                  HitResult* out, int max_out);
```

**Tuning constants** (add to `tuning.h` in a "Weapon contact" block):

| Constant | Value |
|---|---|
| `WEAPON_SWEEP_STEP_DEG` | 4.0f |
| `WEAPON_BOUNCE_RESTITUTION` | 0.4f |
| `DUMMY_RESIST_SMALL` | 45.0f |
| `DUMMY_RESIST_MEDIUM` | 70.0f |
| `DUMMY_RESIST_LARGE` | 130.0f |
| `MOMENTUM_REF` | 80.0f (a bat at 20°/frame at the tip is about 79.6) |
| `DAMAGE_PER_MOMENTUM` | `(1.0f / MOMENTUM_REF)` |
| `HIT_SCALE_MIN` | 0.5f |
| `HIT_SCALE_MAX` | 1.5f |

**Penetration checks** (r_tip ≈ 228 px; m = mass·ω·π/180·r_tip):

| Case | Momentum m | Resistance | Outcome |
|---|---|---|---|
| Chicken, 30°/frame vs small | 47.8 | 45 | through |
| Chicken, 20°/frame vs small | 31.8 | 45 | bounce |
| Chicken, 30°/frame vs medium | 47.8 | 70 | bounce |
| Bat, 15°/frame vs small | 59.7 | 45 | through |
| Bat, 10°/frame vs small | 39.8 | 45 | bounce (it needs a brisk crank) |
| Sledge, 8°/frame vs medium | 83.1 | 70 | through |

**Algorithm for `contact_sweep`** (not set by the signature, so it is spelled out here):

1. **Candidates** are dummies that pass the old gate:
   - alive;
   - not EMERGING, RAGDOLL or DEAD;
   - `0 < d->z − cam->player_z <= w->reach`;
   - their boxes come from `dummy_hit_boxes`, using the same pose choice as `draw_world.c:67-68` (`pose_for_reaction` when `reaction_t > 0`, otherwise `pose_for_state`);
   - `dummy_hit_boxes` returns true.
2. **Overlap** means the grip→tip segment at an angle intersects a box. Use Liang–Barsky clipping. The contact point is the midpoint of the clipped part, and `r` is its distance from the pivot.
3. **Not swept, or not moving.** If `!wp->swept` or `angle == prev_angle`, set `inside` from overlap at `angle` and return 0. This covers docked, teleport, loose, reconnect, shift-only and aim-only frames.
4. **Samples.** Take `n = max(1, ceil(|angle − prev_angle| / WEAPON_SWEEP_STEP_DEG))` samples, `a_k = prev_angle + (angle − prev_angle)·k/n` for k = 0..n.
   - A candidate already in `inside`, or overlapping at k = 0, is not an entry.
   - Otherwise its entry is the first k in 1..n where either box overlaps.
   - The hit is a head hit if the head box overlaps at any k from entry to n. In that case the contact point and `r` come from the head's first overlap. Otherwise they come from the body's.
5. **Order and outcome.** Sort the entries by k (ties broken by dummy index), and process them in order while `out` has room and the weapon is attached:
   - `m = contact_momentum(w, wp->vel, r)`, `R = dummy_resist(size)`, `damage = m·power·(head ? HEAD_MULT : 1)·DAMAGE_PER_MOMENTUM`, `k = clampf(m / MOMENTUM_REF, HIT_SCALE_MIN, HIT_SCALE_MAX)`.
   - Apply exactly as the old `combat_resolve` did, with `factor` replaced by `k`: hp, reaction (`contact_reaction(a_k, vel)`), `reaction_t`, push-back `DUMMY_HIT_PUSHBACK·k`, then ragdoll with launch or stagger.
   - Fill `HitResult` including `sx, sy`.
   - **Launch vector:**
     - the screen tangent is `(cos a, sin a)·sign(vel)`;
     - world `x = tangent.x · 0.5f`, `y = max(−tangent.y, 0.3f)`, `z = 0.9f`;
     - normalise and scale by `w->launch·k`.
   - **Through:** if `m >= R`, or the hit knocked the dummy out, set `wp->vel *= max(0, 1 − R/m)` and continue.
   - **Bounce:** otherwise call `weapon_detach(wp, a_k, −wp->vel·WEAPON_BOUNCE_RESTITUTION)` and stop processing.
6. **Inside refresh.** Recompute `wp->inside` from overlap at the final `wp->angle` (the contact angle after a bounce).

**`contact_reaction(angle, vel)`:** with tangent `(cos a, sin a)·sign(vel)`:
- if |tx| ≥ |ty|: `tx > 0` → `RX_FROM_LEFT`, otherwise `RX_FROM_RIGHT`;
- otherwise: `ty < 0` → `RX_UPWARD`, otherwise `RX_DOWNWARD`.

**Test scenes.** Build each scene with a helper:
1. Spawn the dummy and get its boxes.
2. Aim the weapon at the box centre: `a* = atan2f(bx − pivot_x, pivot_y − by)` in degrees.
3. `CHECK` that the centre's distance from the pivot is within [`WEAPON_SWEEP_RADIUS`, `weapon_tip_radius`], so the scene is valid.
4. Set `prev_angle = a* − 30`, `angle = a* + 10`, `vel` to the case's speed, and `attached = swept = live = true`.

Write the momentum checks at the tip with `contact_momentum(w, vel, weapon_tip_radius(wr))` against `dummy_resist`.

- [ ] **Step 1: Write the failing tests** in `tests/test_contact.c`.

  | Test | Expected result |
  |---|---|
  | `test_balance_checks` | All six rows of the penetration table, using `contact_momentum` at the tip against `dummy_resist`. |
  | `test_normal_speed_matches_old_damage` | Bat at 20°/frame at the tip, a body hit → damage within 2% of `power` (1.0). |
  | `test_reaction_from_tangent` | `contact_reaction(0, +20) == RX_FROM_LEFT`, `(0, −20) == RX_FROM_RIGHT`, `(90, −20) == RX_UPWARD`, `(90, +20) == RX_DOWNWARD`. |
  | `test_sweep_hits_dummy_it_crosses` | Bat at 30°/frame through a small dummy → 1 hit, the hit dummy is STAGGER (or RAGDOLL), `sx/sy` lie inside one of its boxes, `wp->attached`. |
  | `test_sweep_misses_dummy_not_crossed` | Sweep from `a*+15` to `a*+40` → 0 hits, hp unchanged. |
  | `test_out_of_reach_not_hit` | Same scene with `d->z` at `w->reach + 0.2f` → 0 hits. |
  | `test_bounce_detaches_and_reverses` | Chicken at 20°/frame through a medium dummy → 1 hit with damage > 0, `!wp->attached`, `wp->vel` has the opposite sign and size `20·0.4`, and `wp->angle` lies between `prev_angle` and the unbounced end. |
  | `test_penetration_slows` | Sledge at 20°/frame through a medium dummy → attached, and \|vel\| is below 20 by the factor `(1 − R/m)` within 0.01. |
  | `test_second_dummy_after_bounce_untouched` | Two small dummies side by side (chicken, slow) → only the first in sweep order is hit, and the second's hp is unchanged. |
  | `test_line_of_dummies_saps_momentum` | Sledge, fast, two medium dummies → both hit, the second's damage is lower. |
  | `test_ko_counts_as_through` | Chicken, slow, a medium dummy with `hp = 0.01f` → knockout, the weapon is still attached, `vel == 0`. |
  | `test_head_anywhere_in_pass` | Sweep crossing the body first and then the head (small dummy, aim set so the head is in the arc) → `h.head`, and the damage is the head contact's value ×`HEAD_MULT`. |
  | `test_one_hit_per_pass` | Two consecutive frames whose sweeps both overlap the same dummy → 1 hit total. A return sweep after the weapon has left all its boxes → hit again. |
  | `test_still_weapon_never_hits` (Review Focus 5) | `swept = false` with overlap → 0 hits, `inside` bit set. Then a swept frame starting inside → 0 hits. Shift-only (change `wp->shift` and keep `angle == prev_angle`) → 0 hits. |
  | `test_max_out_respected` (Review Focus 4) | Sledge, fast, three dummies in the arc, `max_out = 1` → returns 1, and the other two have full hp and state `DST_WALK`. |

- [ ] **Step 2: Run `make test` and confirm it fails.** Expected: a compile error, because `contact.h` is not found.
- [ ] **Step 3: Implement `contact.c`** and add `sx, sy` to `HitResult`.
- [ ] **Step 4: Run `make test` and confirm it passes.**
- [ ] **Step 5: Commit.** `git add -A && git commit -m "contact: swept weapon vs projected dummy boxes, penetrate or bounce"`

---

### Task 5: Wire it in, draw it, remove the old swing

**Files:**
- Delete: `src/core/swing.h`, `src/core/swing.c`, `tests/test_swing.c`. Move its Task 1 table checks into `tests/test_weapon_motion.c`.
- Modify:
  - `src/core/game.h`: replace `Swing swing` with `Weapon wp`; remove `prev_docked`; include `weapon_motion.h`.
  - `src/core/game.c:226-231,297-321,385`, and `spawn_impacts` at about line 169.
  - `src/core/combat.h/.c`: remove `combat_resolve`, `combat_reaction`, `launch_vector` and `head_aim`; keep `ReactionType`, `HitResult` and `combat_reach_area`.
  - `src/core/weapons.h/.c`: remove `delay` and `arc`.
  - `src/core/draw_world.h/.c`
  - `src/core/tuning.h`
  - `tests/test_combat.c`, `tests/test_draw_world.c:215-310`, `tests/test_particles.c:148`, `tests/test_ragdoll.c:451`, `tests/test_game_flow.c:118-140`, `tests/main.c`
  - `docs/playtest.md`

**Interfaces:**
- Consumes: everything from Tasks 2–4.
- Produces:
  - `void draw_weapon(RenderList* rl, const Weapon* wp, const WeaponRig* wr, int image);`
  - `void combat_reach_area(const WeaponDef* w, int side, float* x0, float* x1, float* z0, float* z1);` keeps its signature. Its x half-width becomes the constant `REACH_PATCH_HALF_W 1.2f` in place of `w->arc`.

**Wiring in `game_update`:**
- **Swing block:**
  1. `weapon_update(&g->wp, in, w)`, then `g->nhits = contact_sweep(&g->wp, w, &g->wrigs[g->weapon], &g->cam, &g->dummies, g->rigs, g->frame, g->hits, 16)`.
  2. The per-hit sounds, scoring and grunt loop is kept as-is.
  3. If the weapon went from attached to loose this frame, push `SND_HIT_LIGHT` (the bounce thunk).
  4. `SND_SWING` plays when |vel| rises past `SPEED_LINE_MIN_VEL` from below.
- **Undock:** the old undock handling (`prev_docked`) is deleted, because `weapon_update` handles undock now.
- **Offer accepted:** `memset(&g->wp, 0, sizeof g->wp)`. The next undocked frame teleports to the crank and is never swept.
- **`spawn_impacts`:** use `h->sx, h->sy` in place of re-projecting the dummy's anchor. `ppm` for the star radius still comes from projecting the dummy.

**Drawing (`draw_world.c`):**
- `polar()` takes the pivot x as a parameter.
- **`draw_weapon`:**
  - Always draws at the wrapped `wp->angle` about `(weapon_pivot_x(wp), WEAPON_PIVOT_Y)`. There's no rest pose and no `sweep_angles`.
  - Speed lines are drawn only when |vel| ≥ `SPEED_LINE_MIN_VEL` (6.0f), trailing opposite `sign(vel)`.
  - A loose weapon is drawn the same way.
- The reach patch keeps following the held side.

**`tuning.h`:**
- Remove `SWING_TRIGGER_DEG`…`SWING_RECOVER_FRAMES`, `WEAPON_REST_X/Y/DEG`, `WEAPON_SIDE_DEG` and `WEAPON_VERT_NEAR/FAR_DEG`, and any `SWING_DELTA_CLAMP`.
- Keep `HEAD_MULT`.
- Set `WEAPON_PIVOT_Y` to 376.0f.
- Add `SPEED_LINE_MIN_VEL` and `REACH_PATCH_HALF_W`.
- `grep -rn "SWING_\|WEAPON_REST\|->delay\|->arc\|Swing\b\|SwingSide" src tests` must print nothing afterwards, except the HUD's own `SWING_CX/CY/R/DOT…` indicator constants. Those stay, because the left/right indicator is unchanged.

- [ ] **Step 1: Rewrite the affected tests first.**
  - **`test_draw_world.c` weapon tests** (replace lines 215–310):

    | Test | Expected result |
    |---|---|
    | `test_weapon_upright_when_docked` | A zeroed `Weapon` → one bitmap at x 200, y `376 − 60`, angle 0. |
    | `test_weapon_follows_angle` | `angle = 30` → the grip lies at `polar(30, 60)`, and the bitmap angle is 30. |
    | `test_weapon_angle_wrapped` | `angle = 390` → the bitmap angle is 30. |
    | `test_pivot_shift_moves_grip` | `shift = −100` → grip x = 100 at angle 0. |
    | `test_speed_lines_only_when_fast` | `vel = 2` → 0 lines. `vel = 20` → `SPEED_LINES` lines, all on the side the weapon came from. |
    | `test_weapon_head_on_screen_upright` | At angle 0 the tip y is inside the screen (≈146 for a 168 px weapon). |

  - **`test_particles.c:148` and `test_ragdoll.c:451`:** replace the swing setup with `g.wp.live = g.wp.attached = true; g.wp.angle = -30; g.wp.vel = 20;`.
  - **`test_game_flow.c`:** the offer test snapshots `g_.wp` in place of `g_.swing`. Add these tests:

    | Test | Expected result |
    |---|---|
    | `test_crank_swing_hits_dummy` | Start, place a medium `DST_WALK` dummy at x 0, z 1.2. Feed undocked frames with `crank_angle = −60` and then `crank_delta = +20` for 8 frames → the dummy's hp drops and `g_.nhits` was 1 on exactly one frame. |
    | `test_accepting_offer_resets_weapon` (Review Focus 2) | Mid-swing, accept an offer → the next frame is `!g_.wp.swept` and causes no hits. |
    | `test_undock_frame_never_hits` | A dummy in the arc, docked frames, then undock with a large `crank_delta` → no hits that frame. |

  - **`test_combat.c`:** remove the tests for the deleted functions. Keep the `combat_reach_area` test, now asserting a width of `2·REACH_PATCH_HALF_W`.
  - **`tests/main.c`:** remove `suite_swing`.
- [ ] **Step 2: Run `make test` and confirm it fails.** Expected: compile errors on `Weapon`/`wp` in `game.h` users and the new `draw_weapon` signature.
- [ ] **Step 3: Make the source changes listed above** and delete `swing.[ch]`.
- [ ] **Step 4: Run `make test && make check-doubles` and confirm both pass.** Then run the grep in the `tuning.h` notes above and confirm it prints nothing beyond the HUD indicator constants.
- [ ] **Step 5: Build and run in the simulator:** `tools/sim.sh`. Check by eye:
  - The weapon is upright and centred when docked.
  - It follows the crank.
  - The sledge visibly trails and the chicken doesn't.
  - Left/right slides it.
  - A slow chicken bounces off a medium dummy, and cranking back reattaches.
- [ ] **Step 6: Update `docs/playtest.md`:**
  - **Controls:** the crank drives the weapon, left/right slide the pivot, up/down aim. Overhead and uppercut swings are gone.
  - **Tuning tables:** mass/spring/damp per weapon, the resistances, and the contact constants.
  - **Feel questions:**
    - Does the sledge lag feel like weight or like delay?
    - Is the bounce readable?
    - Is reconnecting obvious?
    - Is the weight bar useful?
    - Does the reach patch still help?
  - **Balance note:** with the spec's damage formula (momentum × power), heavier weapons now deal `mass` times more per hit than before at the same crank speed, for example sledge ×2.6. Flag hits-to-kill for retuning `power` in playtest.
- [ ] **Step 7: Commit.** `git add -A && git commit -m "game: crank-driven weapon replaces canned swing"`
