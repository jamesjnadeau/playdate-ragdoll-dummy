# Ragdoll Dummies: playtest guide

This is a playtest build. The art is placeholder. What I need from you is how it feels.

## Getting it on your Playdate

Option 1, sideload from the web:
1. Download the CI artifact `RagdollDummies.pdx.zip` (zipped `RagdollDummies.pdx`).
2. Go to https://play.date/account/sideload and upload the zip.
3. On the Playdate, open Settings → Games and refresh; the game downloads over Wi-Fi and appears in your library.

Option 2, USB:
1. Connect the Playdate over USB, then on the Playdate choose Settings → System → Reboot to Data Disk.
2. Unzip, then copy the `RagdollDummies.pdx` folder into the `Games` folder on the Playdate volume.
3. Eject the disk. The game shows up in the launcher.

## Controls

| Input | Action |
|---|---|
| A / B | Walk forward / back |
| Crank | Drives the weapon. It follows the crank on a spring, so heavy weapons lag behind |
| D-pad left / right | Slide the weapon's pivot left / right (and move the floor reach hint to that side) |
| D-pad up / down | Aim up / down |
| Offer screen: A | Take the new weapon |
| Offer screen: B | Keep your current weapon |

The weapon points where the crank points (straight up when the crank is at 0). Docking the crank stands the weapon upright. A swing hits whatever the weapon passes through, and faster cranking hits harder. There are no separate overhead or uppercut swings any more: the direction you crank is the direction of the hit.

A hit with enough momentum goes through the dummy and carries on, a little slower. A hit without enough momentum bounces: the weapon flies back loose and stops following the crank. Bring the crank back to the weapon's angle to pick it up again. The rubber chicken is the exception: it is too floppy to bounce, so it always passes straight through at full speed.

## What to try

- Swing both ways on the crank, with the pivot left, centred and right.
- Make a weapon bounce (a slow bat swing does it), then reconnect with it.
- Whip the chicken through a crowd: it never bounces, so it should flop through every dummy in its path.
- Use every weapon: Rubber Chicken, Bat, Frying Pan, Sledgehammer. Weapons are offered as pickups as you pass doors.
- Wall bursts: hit a dummy hard so the ragdoll slams a wall and bursts.
- Bowling: knock a ragdoll into other dummies.
- Door slams: send a ragdoll into a door.
- Let a big crowd build up, take damage, die, and restart. Check that the best score is kept after you quit and reopen.

## Feel questions

1. Does the sledgehammer's lag feel like weight, or like delay?
2. Is the bounce readable: can you tell the weapon came loose, and why?
3. Is reconnecting obvious: do you find the weapon again without thinking about it?
4. Is the weight bar on the offer screen useful?
5. Does the floor reach patch still help, now that hits come from where the weapon actually is?
6. Are the dummy reactions readable?
7. Is the ragdoll too bouncy?
8. Is the difficulty curve right: too fast, too slow, too crowded?
9. The heavy hit sound is now sledgehammer-only: it plays when the weapon's mass is at least HIT_HEAVY_MASS (only the sledge) and the hit's momentum is at least HIT_HEAVY_MOMENTUM. This is a deliberate change: before the crank weapon, the frying pan and a fast bat hit also played the heavy sound. Every pan and bat hit now sounds light. Do you miss the heavy sound on the pan or a fast bat?

Also tell me about anything that stutters, anything that looks wrong, and any crash.

## Tuning values

All gameplay numbers are in `src/core/tuning.h`, except the weapon table in `src/core/weapons.c`. These are the ones most likely to change.

| Value | Current | Meaning |
|---|---|---|
| WEAPON_MAX_VEL | 40 deg/frame | Top weapon speed |
| SPEED_LINE_MIN_VEL | 6 deg/frame | Faster than this draws speed lines and plays the swing sound |
| DUMMY_RESIST_SMALL / MEDIUM / LARGE | 45 / 70 / 130 | Momentum a hit needs to go through; less bounces |
| MOMENTUM_REF | 80 | Momentum for a full-size hit reaction (a bat at 20 deg/frame hitting near its tip is about 80) |
| DAMAGE_PER_MOMENTUM | 1/80 | Damage = momentum × power × this (× HEAD_MULT on the head) |
| HEAD_MULT | 1.5 | Head hit damage multiplier |
| HIT_HEAVY_MASS / HIT_HEAVY_MOMENTUM | 2.0 / 40 | A hit plays the heavy sound when the weapon is at least this heavy (only the sledge) and the contact momentum is at least this. The pan and a fast bat used to sound heavy and no longer do (feel question 9) |
| WEAPON_BOUNCE_RESTITUTION | 0.4 | Share of the speed a bounced weapon flies back with |
| WEAPON_BOUNCE_DAMP | 0.85 | Loose weapon speed kept per frame |
| WEAPON_RECONNECT_DEG | 3 | How close the crank must come to a loose weapon to pick it up (crossing it also counts) |
| WEAPON_SWEEP_STEP_DEG | 4 | Spacing of the contact samples along a frame's sweep |
| WEAPON_SHIFT_PX | 100 px | How far left / right slides the pivot |
| REACTION_FRAMES | 12 | How long a dummy's hit reaction lasts |
| RAGDOLL_RESTITUTION | 0.45 | Bounciness (higher is bouncier) |
| RAGDOLL_IMPACT_MIN | 3.0 m/s | Below this speed a ragdoll takes no impact damage |
| RAGDOLL_HP | 6 | Ragdoll health before it bursts |
| RAGDOLL_LAUNCH_SPIN | 0.35 | Head/base opposite sideways velocity, as a fraction of launch speed |
| DUMMY_HP_SMALL / MEDIUM / LARGE | 2 / 3 / 5 | Base dummy health |
| DUMMY_HP_PER_100M | 0.25 | Extra health fraction per 100 m of distance |
| MAX_LIVE_DUMMIES | 10 | Doors hold new dummies while this many are alive |
| WALK_BACK_LIMIT | 3.0 m | How far you can walk back from the nearest open door |
| DOOR_OPEN_MAX_DEG | 165 | Fully open door angle |
| PLAYER_RADIUS | 0.35 m | Blocking distance is PLAYER_RADIUS + dummy radius |
| DUMMY_RADIUS_SMALL / MEDIUM / LARGE | 0.30 / 0.38 / 0.50 m | So you stop 0.65 / 0.73 / 0.85 m from a dummy |
| HORIZON_Y | 72 px | Screen row of the horizon at middle aim (was 108). Lower values tilt the view down: more floor (and the weapon range patch) and more of close dummies' bodies show |
| WEAPON_PIVOT_X / Y | 200 / 376 px | Where the weapon pivots (below the screen). Raise Y to move the whole weapon lower. Upright, the bat's head is at about y 147 |
| WEAPON_SWEEP_RADIUS | 60 px | Grip distance from the pivot. Upright, the grip is at PIVOT_Y minus this, so a smaller value lifts the weapon |

The blocking distance matters because near dummies only show their head on screen. Raise or lower the radii to change how much of them you see.

The floor shows a light dotted patch as a reach hint (the current weapon's reach, 2.4 m wide, shifted toward the side you hold on the D-pad). It is only a hint: hits come from the weapon touching a dummy on screen. The patch starts about y 200 at the default aim (it is partly under the bottom HUD panel on the left).

Weapon table (`src/core/weapons.c`): fields are name, folder, power, reach, launch, mass, spring, damp, weight bars, power bars. Mass is how much the weapon lags the crank and how hard it hits; spring and damp set how it catches up.

| Weapon | Power | Reach | Launch | Mass | Spring | Damp |
|---|---|---|---|---|---|---|
| Rubber Chicken | 1.5 | 1.3 | 3.0 | 0.4 | 0.4 | 1.0 |
| Bat | 1.0 | 1.6 | 5.0 | 1.0 | 0.5 | 0.77 |
| Frying Pan | 1.01 | 1.4 | 6.0 | 1.4 | 0.5 | 0.67 |
| Sledgehammer | 0.92 | 1.8 | 9.0 | 2.6 | 0.5 | 0.49 |

**Balance note.** Damage = momentum × power × DAMAGE_PER_MOMENTUM, and momentum already includes the weapon's mass, so `power` is per unit of momentum. It is set so a body hit at the tip at 20 deg/frame does what the old canned swing did at power factor 1.0, and hits-to-kill match the old game (small / medium / large: 2 / 3 / 5 HP):

| Weapon | Damage per tip hit at 20 deg/frame | Hits to kill |
|---|---|---|
| Rubber Chicken | 0.60 | 4 / 5 / 9 |
| Bat | 1.00 | 2 / 3 / 5 |
| Frying Pan | 1.40 | 2 / 3 / 4 |
| Sledgehammer | 2.39 | 1 / 2 / 3 |

Hits nearer the grip, or slower ones (the sledge lags the crank), do less. Faster ones do more. Note how many hits each weapon takes in practice.

## Known rough edges

- All art is placeholder.
- The worst frame in the perf scenario "wave" is about 29.7 ms on the simulator (pdsim), just under the 33 ms budget. Average is about 15 ms. Real hardware may differ, so tell me if you see slowdowns in a big crowd.
- Rotated sprites use a rotation cache, so you may see the angle step in visible increments.
- Community skins are not loadable yet.
