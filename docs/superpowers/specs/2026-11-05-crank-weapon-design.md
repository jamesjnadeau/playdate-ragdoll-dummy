# Crank-driven weapon: design

Date: 2026-11-05. Status: draft, waiting for the owner's approval.

## Intent

Swinging should feel like your crank is the weapon. Today a swing is a canned animation. A crank flick past a threshold starts it, a per-weapon delay holds it, it plays a fixed 6-frame sweep that ignores the crank, and one instant hit lands at the midpoint on a reach box. The owner said this feels disconnected, and in particular "there's still a delayed reaction based on weapon".

What the owner wants:

- The crank sets the weapon's position on screen, directly and continuously. Nothing starts or ends a swing at a crank-speed threshold.
- Heavier weapons lag behind the crank; light ones follow it almost exactly. (Asked directly, the owner chose "heavy ones lag".) The lag is weight, not a wait: the weapon starts moving the same frame the crank does, but a heavy one speeds up slowly, trails behind and carries on a little when the crank stops. The old fixed per-weapon delay before anything moves is what the owner complained about and must not come back.
- Damage depends on how fast the weapon is moving, which comes from how fast the crank is turning.
- There's no single hit moment. The weapon sweeps across whatever it's aimed at, in the direction the crank is turning, and physically meets the dummies.
- On contact:
  - **Not heavy enough to get through:** the weapon bounces back, slows down and comes loose from the crank. The player brings the crank back to the other side to reconnect with the weapon, then swings again.
  - **Enough penetration:** the weapon goes through.
- Move the weapon to the centre of the screen and up 10% from where it is now.

## Goals

- What you see the weapon touch is what gets hit, at the spot where it touches.
- Light weapons with a slow crank bounce off. Heavy weapons, or a fast crank on a small dummy, go through.
- Runs at 50 fps on the device, with no double-precision math (the existing `-Werror=double-promotion` flags and `tools/check_no_doubles.sh` still apply).
- The core stays pure C and host-testable (`make test`).

## Non-goals

- A true 3D weapon or 3D collision. Depth is only a reach check.
- New weapon art. The existing held-weapon bitmaps and `WeaponRig` grip and tip points are reused.
- Changing dummy AI, ragdolls, particles or the hallway.

## Anti-goals

These would make it a failure even if it technically works:

- Any frame where the crank moves and the weapon doesn't start to move. Lag comes from weight only, never from a wait.
- A light weapon (chicken) that visibly trails the crank.
- A hit the player didn't see the weapon touch, or a visible touch that does nothing (for an in-reach dummy).
- A swing that still feels like a fixed animation.

## Constraints

- Playdate C SDK 3.1.2. Crank input today is only `getCrankChange()`. The platform layer (`src/platform/playdate/pd_input.c`) adds `getCrankAngle()` and passes it into `GameInput` as a new `crank_angle` field.
- The screen is 400×240.
- Floats only, with `f` suffixes, as everywhere else in the core.

## Approaches considered

1. **Hit what's on screen (picked by the owner).** The weapon is a 2D picture rotating about a pivot below the screen. Each frame, collision sweeps the weapon's on-screen shape from last frame's angle to this frame's and tests it against each in-reach dummy's projected part boxes. *Trade-off:* exact visual agreement and cheap math, but depth is only a reach gate.
2. **A real 3D weapon.** A rod swings in world space, collides with 3D dummy volumes and is projected for drawing. *Trade-off:* proper depth, but the drawn weapon and the collision don't line up exactly. It's also more rework and more math per frame. Rejected because it breaks "what you see touch is what hits".
3. **Crank angle windows per dummy.** Rejected: it's approach 1 with coarser shapes, and it loses the hit spot.

## Design

### Placement and mapping

- The weapon rotates about a pivot at **(200, 376)**. Left and right shift the pivot's x (see *Inputs*). That puts it in the horizontal centre, 24 px (10% of the 240 px screen height) higher than the current pivot at (200, 400). The grip sits `WEAPON_SWEEP_RADIUS` (60 px) from the pivot along the weapon's direction, as it does today. At angle 0 the weapon points straight up, its grip is off the bottom edge, and the head lands about 24 px higher than today's mid-sweep (about y 146 instead of 170).
- The rest pose at (352, 300), −53° goes away. The weapon is always on the arc.
- **Mapping:** the *crank target angle* is `crank_angle − CRANK_ZERO_DEG`, with gain 1. It is kept **unwrapped**: it starts from `crank_angle − CRANK_ZERO_DEG` on undock and then accumulates `crank_delta` each frame, so a weapon lagging a long way behind still turns the same way as the crank. The weapon angle is unwrapped too, and is wrapped only for drawing and collision. With `CRANK_ZERO_DEG = 0`, the crank arm pointing up puts the weapon upright in the centre.
  - Crank angle increases clockwise as seen from the right side, so turning one way sweeps the weapon left to right over the top, and the other way right to left.
  - Over a full turn the weapon goes all the way round the pivot: it sweeps across the screen on the top half and passes under the screen on the bottom half. Continuous cranking gives continuous swings, one pass across the dummies per turn.
- **Docked crank:** the weapon is drawn upright at the centre (angle 0) and can't hit anything. On the frame the crank undocks, the weapon attaches at the crank's angle. That jump is a teleport, not a sweep, and is never collided.

### Weapon state

Replace `Swing` with a `Weapon` state in the core:

- `angle` and `prev_angle`, in degrees on the arc.
- `vel`: degrees per frame, signed.
- `attached` (bool).
- A per-dummy "inside" flag (or a small set of dummy ids) recording which dummies the weapon currently overlaps. See *One hit per pass*.

Each frame:

- **Attached (spring-coupled to the crank):** the weapon is pulled toward the crank target angle by a spring, and its weight resists it.
  - `err = target − angle`
  - `vel += err × WEAPON_SPRING / w->mass − vel × WEAPON_DAMP`
  - `vel` is clamped to ±`WEAPON_MAX_VEL` (about 40°/frame) so a crank whip can't blow up.
  - `angle += vel`
  - The tuning targets are about how long the weapon takes to catch up once the crank stops after a steady turn: chicken at most 2 frames, which looks instant; bat about 4; pan about 6; sledge about 10 (0.2 s). The sledge may overshoot slightly before settling, as follow-through. The builder picks `WEAPON_SPRING` and `WEAPON_DAMP` (or per-weapon values) to meet these and writes them as host tests.
  - A weapon at rest with the crank moving must have non-zero `vel` on that same frame.
- **Detached:** `angle += vel`, and `vel *= WEAPON_BOUNCE_DAMP` (about 0.85, a tuning value). Once |vel| drops below a small threshold, `vel` is set to 0.
- **Reconnect:** a detached weapon re-attaches when the crank target angle crosses or meets the weapon's angle. The test is a sign change of the wrapped difference between them from one frame to the next, or a difference under 3°.
  - After a bounce, the crank is past the contact point and the weapon has bounced back toward the start side. Turning the crank back meets the weapon; turning it forward a full turn meets it as well.
  - On re-attach the weapon snaps to the crank, which is within a few degrees.

### Collision: swept weapon versus projected part boxes

The test runs every frame the weapon is attached and |vel| > 0.

1. **Weapon shape.** The weapon is a line segment from the grip to the tip in screen space, taken from `WeaponRig` (grip and tip in image pixels, rotated by `angle` and placed so the grip sits on the arc). The swept area between `prev_angle` and `angle` is a wedge about the pivot.
2. **Candidates.** Live dummies with `dz` in (0, `w->reach`] that aren't EMERGING, RAGDOLL or DEAD, which is the same gate `combat_resolve` uses today.
3. **Targets.** For each candidate, compute screen-space boxes for its **head** and **body** using the camera projection and pose that `draw_dummy` already uses. Arms and fists count as body.
4. **Contact test.** Find the first angle in the sweep at which the weapon segment touches a box. Use the box corners' angles about the pivot, combined with their distance from the pivot compared with the segment's inner and outer radius. Sampling the sweep in steps of at most 4° is acceptable if exact is awkward; that's the builder's call. Process the contacts in sweep order, nearest the start first.
5. **Contact point and speed.** The contact point is where the segment meets the box. Contact speed is `|vel| × (π/180) × r`, in px per frame, where `r` is the contact point's distance from the pivot. Hits nearer the tip are faster.

### Penetration and bounce

For each contact, in sweep order:

- **Momentum:** `m = w->mass × contact_speed`.
- **Dummy resistance:** `R` comes from the dummy's size, `DUMMY_RESIST_SMALL/MEDIUM/LARGE`.
- **Damage:** `m × w->power × (head ? HEAD_MULT : 1) × DAMAGE_PER_MOMENTUM`, applied in either case below. Knockout, ragdoll launch, reaction pose, stagger, push-back, sounds, impact stars and teeth all keep their current behaviour, but each now scales with momentum instead of the old `factor`.
- **Knock direction:** the weapon's screen tangent at the contact point, sign set by `vel`, is mapped to world x/y. Forward z is added as `launch_vector` does today. Its size is `w->launch × m / MOMENTUM_REF`, clamped.
- **If `m ≥ R` (penetrates):**
  - The weapon stays attached, but it slows: `vel *= (1 − R / m)`, and the momentum left for the next contact in the same sweep drops to match. A strong swing can go through a line of dummies, each one sapping it. The spring then pulls it back up to the crank, so a heavy weapon visibly drags through a big dummy.
- **If `m < R` (bounces):**
  - The weapon stops at the contact angle and detaches.
  - `vel = −vel × WEAPON_BOUNCE_RESTITUTION` (about 0.4), and later contacts in that sweep are ignored.
  - It plays the existing `SND_HIT` or a new thunk if one exists. A new sound isn't required.
  - A detached weapon can't hit anything.

### One hit per pass

While the weapon stays inside a dummy's boxes over several frames, the dummy is hit only once, on entry. It can be hit again once the weapon has left all of its boxes, for example on the return stroke.

### Weapon stats

`WeaponDef` changes:

- `delay` is removed.
- `arc` is removed. Hits now come from the drawn shape.
- `mass` is added.
- `power`, `reach` and `launch` are kept.
- The offer screen's speed bar becomes a weight bar (derived from `mass`) or is dropped. The builder chooses, and it's flagged in the playtest doc.

Starting values, to be tuned in playtests:

| Weapon | mass | power | reach | launch |
|---|---|---|---|---|
| Chicken | 0.4 | 0.6 | 1.3 | 3.0 |
| Bat | 1.0 | 1.0 | 1.6 | 5.0 |
| Pan | 1.4 | 1.4 | 1.4 | 6.0 |
| Sledge | 2.6 | 2.4 | 1.8 | 9.0 |

The resistances are set so that:

- The bat needs a brisk crank (about 15°/frame at the tip) to get through a small dummy.
- The sledge goes through a medium dummy at a slow crank.
- The chicken bounces off anything larger than small, and gets through a small dummy only at a fast crank (30°/frame at the tip).

The builder picks the numbers to meet those three checks and writes them as host tests.

### Inputs

- **Crank:** drives the weapon.
- **Up and down:** keep changing the camera aim (low, mid, high). That's how the player chooses whether the arc crosses heads or bodies.
- **Left and right (owner's call):** shift the weapon sideways so you can work different parts of the screen.
  - Holding left eases the pivot to the left position. Holding right eases it to the right position. With neither held, it eases back to the centre. This is the same held-button feel as today's swing sides.
  - The pivot x moves to `200 − WEAPON_SHIFT_PX` or `200 + WEAPON_SHIFT_PX`. `WEAPON_SHIFT_PX` starts at about 100 and is a tuning value.
  - It eases at `WEAPON_SHIFT_PX_PER_FRAME`, like the camera aim does.
  - Moving the pivot moves the whole weapon, but the shift on its own never causes a hit. As with call 8, only the weapon's swing angle hits; overlap from the shift only updates the "inside" flags.
  - Collision always uses the current pivot.
- **Floor reach patch:** `draw_reach_patch` keeps taking the side from held left/right, so it follows the shifted weapon. The builder may centre it under the pivot's actual x instead, or drop it if it no longer reads well.
- **Vertical (overhead or uppercut) swings:** these go away. There's one arc, and its direction comes from the crank's direction.

### Drawing

- `draw_weapon` draws the bitmap at the arc position for `angle`, with no rest pose.
- Speed lines are drawn behind the weapon when |vel| is above a threshold, in the direction of travel. That's the current look, driven by `vel`.
- A detached weapon is drawn in the same way, so the bounce is visible.

### Code touched

| File | Change |
|---|---|
| `src/core/swing.[ch]` | Replaced by a weapon motion module (`weapon_motion.[ch]` or similar): state, attach and detach, reconnect. |
| `src/core/combat.[ch]` | `combat_resolve` becomes a swept-contact resolver taking the weapon state and the camera. `combat_reaction` maps the tangent direction to a `ReactionType`. |
| `src/core/weapons.[ch]` | The new fields and values. |
| `src/core/input.h`, `src/platform/playdate/pd_input.c` | Add `crank_angle`. |
| `src/core/draw_world.c` | Reworked `draw_weapon`; reach patch. |
| `src/core/game.c` | Wiring. Up and down still set the aim. |
| `src/core/tuning.h` | Swing constants out; weapon motion and contact constants in; pivot at (200, 376). |
| `tests/test_swing.c`, `tests/test_combat.c` | Rewritten for the attach, detach and reconnect rules, swept contact, and penetrate versus bounce. |
| `docs/playtest.md` | Updated controls, tuning tables and feel questions. |

## Calls made by the designer (owner to check)

1. The pivot moves up 24 px to (200, 376) and stays centred. "Up 10%" is read as 10% of screen height, applied to the whole arc.
2. The crank maps to weapon angle 1:1 around the full circle, with crank up meaning weapon upright. The back half of a turn passes under the screen.
3. A penetrating hit slows the weapon by the share of momentum the dummy soaked up; the spring then catches it back up to the crank.
4. Reconnect happens when the crank angle meets the weapon's angle from either direction.
5. Left and right shift the weapon's pivot sideways (owner's call). The pivot x and the easing are mine: held, eased, and back to centre on release. Overhead and uppercut swings are gone.
6. Weapon lag is a spring with weight, not a delay. Chicken effectively instant, sledge about 0.2 s to catch up, with a little overshoot. Contact speed is the weapon's own speed, so a heavy weapon that has built up speed keeps hitting hard for a moment after the crank slows.
7. **Head hits:** a pass counts as a head hit if the weapon touches the head box at any point during it, even when it met the body box first. Otherwise sideways sweeps would almost never score head hits, because the body box is wider. The contact point and speed come from the head contact.
8. **Dummies moving into a still weapon:** only the weapon's own motion hits. A dummy that walks into a weapon that isn't moving, or a box that moves because the camera aim changed, causes no hit. The "inside" flags still update from overlap every frame, so the next real swing out of the dummy doesn't count as an entry.
9. **A bounce that knocks the dummy out counts as getting through.** Damage is worked out first. If it knocks the dummy out, the dummy ragdolls and the weapon carries on attached, with momentum reduced by `R`.
10. **Balance:** `DAMAGE_PER_MOMENTUM` is set so that a contact at "normal" speed (20°/frame at the tip) does about what today's factor 1.0 hit does, which keeps hits-to-kill close to today's. "Slow" means 8°/frame at the tip and "fast" means 30°/frame. The chicken gets through a small dummy at fast speed; the masses can move to make that true.

## Left to the builder

- Exact versus sampled sweep collision (with a step of at most 4°).
- Resistance and momentum constants that meet the three penetration checks.
- Speed-line threshold, bounce damping, the reconnect tolerance, and the spring and damping constants (within the catch-up targets).
- Whether the offer screen shows a weight bar or drops the speed bar.
- Whether the reach patch stays.
