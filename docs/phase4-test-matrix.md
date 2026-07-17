# Phase 4 manual test matrix

Run `./build/reflex_engine assets/levels/test_scene.glb`, click to capture the
mouse, and use F4 to inspect gameplay bounds. Repeat timing-sensitive checks
once with VSync and once with VSync unavailable or under a compositor.

## Interaction and doors

- Aim at `door_direct` within 2.75 m: prompt appears and E opens it once per key press.
- Aim from beyond range and through a collidable wall: no prompt or activation.
- Use `switch_security`: `door_switch` toggles and repeated events remain stable.
- Enter/leave `trigger_auto_door`: `door_auto` opens once on enter and closes on exit.
- Stand in a closing door: it reports Blocked, waits briefly, and reopens.
- Confirm the player cannot cross a closed door and F4 collision follows its visual.
- Activate `switch_one_shot`, leave, and return: it does not fire again.

## Keys and pickups

- Use `door_locked_blue` without the key: locked feedback appears.
- Walk over `pickup_blue_key`: the key HUD appears and the pickup disappears.
- Use the locked door again: it unlocks and opens; the key is retained.
- Damage the player, collect `pickup_health_small`, and confirm clamped healing.
- At 100 health, confirm the health pickup remains. Repeat for armor at 100.
- Confirm every pickup collects once only.

## Damage, death, and checkpoints

- Enter `damage_lava`: health falls at the same rate at low and high render FPS.
- Confirm armor absorbs half until depleted when `bypass_armor` is false.
- Enter `checkpoint_after_lava`, then die: movement stops for two seconds and
  respawn occurs at that checkpoint with cleared velocity and configured vitals.
- Before activating it, confirm death falls back to `player_spawn`/start checkpoint.

## Persistence and reset

- Open doors, activate a one-shot switch, collect the key, change vitals, and
  activate the second checkpoint. Press F5.
- Alter all those states, move/look elsewhere, then press F9. Confirm player,
  view, vitals, keys, checkpoint, doors, switches, and pickups restore together.
- Press F6 and confirm authored initial state without reloading the GLB.
- Corrupt the JSON and test `format_version: 999`: both fail safely and preserve
  the live level state.
- Stand for several minutes and repeat at low/high render rates; triggers must
  not repeat enter events, doors must stay synchronized, and shutdown must remain clean.
