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
| D-pad left / right | Choose the swing side |
| D-pad up / down | Aim up / down |
| Crank | Swing. Cranking forward is a downward swing, backward is an upward swing. |
| Offer screen: A | Take the new weapon |
| Offer screen: B | Keep your current weapon |

A swing starts when the crank turns far enough in one frame. Faster cranking hits harder, within limits.

## What to try

- Swing in every direction: left, right, up and down, forward and backward on the crank.
- Use every weapon: Rubber Chicken, Bat, Frying Pan, Sledgehammer. Weapons are offered as pickups as you pass doors.
- Wall bursts: hit a dummy hard so the ragdoll slams a wall and bursts.
- Bowling: knock a ragdoll into other dummies.
- Door slams: send a ragdoll into a door.
- Let a big crowd build up, take damage, die, and restart. Check that the best score is kept after you quit and reopen.

## Feel questions

1. Is the crank threshold too twitchy or too stiff?
2. Is the swing delay right for each weapon?
3. Are the dummy reactions readable?
4. Is the ragdoll too bouncy?
5. Is the difficulty curve right: too fast, too slow, too crowded?

Also tell me about anything that stutters, anything that looks wrong, and any crash.

## Tuning values

All gameplay numbers are in `src/core/tuning.h`, except the weapon table in `src/core/weapons.c`. These are the ones most likely to change.

| Value | Current | Meaning |
|---|---|---|
| SWING_TRIGGER_DEG | 12 | Crank degrees in one frame to start a swing (lower is twitchier) |
| SWING_SPEED_REF | 30 | Crank deg/frame that gives power factor 1.0 |
| SWING_FACTOR_MIN / MAX | 0.5 / 1.5 | Clamp on the swing power factor |
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
| WEAPON_PIVOT_X / Y | 200 / 400 px | Where the swing pivots (below the screen). Raise Y to move the whole sweep lower |
| WEAPON_SWEEP_RADIUS | 60 px | Grip distance from the pivot. Mid-sweep the grip is at PIVOT_Y minus this, so a smaller value lifts the weapon |
| WEAPON_REST_X / Y / ANGLE | 352 / 300 px / -53 deg | Where the weapon rests (grip position and tilt). Raise Y to hide more handle |

The blocking distance matters because near dummies only show their head on screen. Raise or lower the radii to change how much of them you see.

The floor shows a light dotted patch where your weapon can hit (reach and arc of the current weapon, shifted toward the side you hold on the D-pad). The camera looks level, so at the default aim only the far edge of it is on screen; aim down (D-pad down) to see more of it.

Weapon table (`src/core/weapons.c`): fields are name, folder, delay (frames), power, reach, arc, launch, speed bars, power bars.

| Weapon | Delay (frames) | Power | Reach | Arc | Launch |
|---|---|---|---|---|---|
| Rubber Chicken | 2 | 0.6 | 1.3 | 1.0 | 3.0 |
| Bat | 5 | 1.0 | 1.6 | 1.2 | 5.0 |
| Frying Pan | 7 | 1.4 | 1.4 | 1.0 | 6.0 |
| Sledgehammer | 12 | 2.4 | 1.8 | 1.4 | 9.0 |

## Known rough edges

- All art is placeholder.
- The worst frame in the perf scenario "wave" is about 29.7 ms on the simulator (pdsim), just under the 33 ms budget. Average is about 15 ms. Real hardware may differ, so tell me if you see slowdowns in a big crowd.
- Rotated sprites use a rotation cache, so you may see the angle step in visible increments.
- Community skins are not loadable yet.
