# Reflex Engine Phase 5 manual test matrix

Run `./build/reflex_engine assets/levels/test_scene.glb`, click to capture the
mouse, and enable F7. Repeat timing checks with VSync on/off or at deliberately
different render rates. Combat simulation remains 120 Hz.

## Weapon handling

- Start with the pistol; walk over the shotgun and plasma pickups, then use
  1/2/3 and the wheel. Confirm each magazine survives a round trip.
- Click once and hold for each semi-automatic weapon. One press produces one
  shot and cooldown prevents excess shots.
- Empty each magazine, verify dry fire produces no muzzle flash, press R, and
  confirm reserve transfers without becoming negative.
- Reject reload at a full magazine or empty reserve. Start reload, switch, and
  confirm the documented cancellation policy.
- Confirm equipping blocks fire and F7 reports state, timer, ammo, RNG, and
  projectile count.

## Hitscan and projectiles

- Shoot a wall and an enemy with the pistol. Confirm nearest hit wins and a wall
  between camera and actor blocks damage.
- Fire the shotgun at close and long range. Inspect eight deterministic pellet
  traces and confirm the disk pattern is not square-biased.
- Fire plasma into walls, doors, and enemies. Confirm visible swept travel,
  one impact, finite lifetime, linear splash falloff, wall-blocked splash, and
  configured self-damage.
- Confirm hit and kill crosshair feedback, muzzle flash only on valid shots,
  bounded line effects, and cleanup after lifetimes.

## Enemy behavior

- Approach each grunt outside then inside its distance/FOV. Confirm reaction
  delay, Alert, Chasing, Attacking, Pain, and one-time Dead transitions.
- Put a wall or closed door between player and grunt; perception and attacks
  must fail. Reopen the door and confirm reacquisition.
- Lead a grunt through open floor and along a wall/corridor. Confirm local slide,
  closed-door blocking, attack-range stop, and stable player blocking.
- Move out of sight and confirm brief last-seen pursuit followed by Idle.
- Take repeated attacks: cooldown, armor absorption, directional HUD feedback,
  death, and checkpoint respawn must remain stable.

## Pickups and persistence

- Collect each weapon/ammo type. A duplicate weapon grants authored reserve;
  full ammo leaves the pickup present.
- Damage/kill enemies, change magazines and equipped weapon, collect combat
  pickups, then F5. Alter all state and F9; verify combat state restoration.
- Confirm living enemies restore to Idle, dead enemies stay dead, pickups remain
  collected, and active projectiles/effects disappear safely.
- Corrupt combat IDs/counts/positions or set a future format version. Loading
  must fail without partially changing the live state.
- Press F6 and confirm weapons, ammo, enemies, pickups, RNG, and authored
  gameplay return to initial state without reparsing the GLB.

## Stability and performance

- Compare fire cadence, reload duration, projectile travel, enemy attack timing,
  and damage at low/high render FPS and after a debugger pause.
- Fight all three generated grunts while firing repeated shotgun/plasma shots;
  bounded projectile/effect counts and clean shutdown must hold.
- Leave the engine running, hold movement against enemies/doors, repeatedly
  save/load, and verify no state oscillation or repeated death processing.
