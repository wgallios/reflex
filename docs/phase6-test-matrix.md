# Phase 6 test matrix

## Automated and headless

- Configure and build `reflex_engine`, `reflex-asset-tool`, and all tests.
- Run CTest, including animation sampling/events/hierarchy, Recast/Detour paths,
  repath timing, encounter waves, objectives, audio policy, manifests, and saves.
- Run the `validate-assets` target and confirm both levels, combat definitions,
  audio, generated animated models, navigation metadata, and manifest pass.
- Confirm malformed/duplicate test inputs are rejected without SDL or OpenGL.

## Skeletal animation

- Load `assets/models/grunt.glb` as a standalone scene and inspect all joint matrices.
- Confirm bind pose, loop wrap, one-shot completion, transformed parents, and the
  128-joint rejection path.
- Replace a clip reference with an unknown name and confirm validation fails.
- Confirm the model remains stationary in world space while the root animates.

## Navigation and enemies

- Inspect startup polygon/build statistics and F8/F10 query counts.
- Pursue around a wall, through two rooms, up valid stairs/ramps, and to an
  unreachable destination (partial/no-path result).
- Close/open a door, confirm collision blocks it, and confirm periodic replanning.
- Test ten and thirty agents in a corridor; inspect stuck time and frame cost.
- Author and validate a one-way drop link and a bidirectional door link.

## Encounters, objectives, and campaign

- Start Level 1 encounter, lock its door, clear the wave, complete the objective,
  and activate the exit.
- Transition to Level 2 at its target spawn with health, armor, weapons, magazines,
  ammunition, and persistent keys intact; transient effects/projectiles must clear.
- Clear both Level 2 waves and complete the campaign objective exactly once.
- Quick-save/load before, during, and after each encounter and across level IDs.
- Reject unknown objectives, encounters, levels, and spawn names before mutation.

## Audio and performance

- Confirm weapon/UI sounds, positional enemy sounds, listener orientation, music
  looping, concurrency rejection, and graceful no-device/missing-file behavior.
- Confirm F10 reports frame/simulation/render/animation/navigation/audio counts.
- Profile 10, 20, and 30 animated enemies at several window resolutions and
  verify no sustained allocation or voice/projectile/particle growth.

## Current generated-content note

The repository includes original generated rigged triangle and weapon-viewmodel
fixtures plus two level definitions. Replace the copied Phase 5 level geometry
with final Blender-authored multi-room campaign scenes before content sign-off.
