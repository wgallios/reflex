# Phase 3 manual collision test matrix

Regenerate the repository test course, build, and launch it:

```bash
python3 tools/generate_test_scene.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/reflex_engine assets/levels/test_scene.glb
```

Press F3 while testing. Record the engine revision, display backend, approximate
render rate, and pass/fail notes. Repeat the stability group once with VSync and
once with VSync disabled locally if frame-rate comparison is needed.

## Basic movement

- Stand still on the large floor; grounded remains `yes` without drift.
- Walk forward/back and strafe; release input and confirm predictable stopping.
- Hold W+D; speed must match straight movement rather than being 1.414x faster.
- Hold Shift while moving; speed increases from 5 m/s to 8 m/s.
- Walk directly into an enclosing wall; the capsule cannot pass through.
- Approach a wall diagonally; motion continues along the wall.
- Move into the marked inside corner; the controller stops without oscillation.

## Vertical behavior

- Spawn and fall the small initial gap onto the floor.
- Press Space once and land; holding Space must not retrigger the jump.
- Walk off the elevated platform and land without tunneling.
- Jump beneath the low-ceiling area; upward motion stops at the ceiling.
- Stand and move under the low ceiling without camera/capsule clipping.

## Slopes

- Walk up and down the shallow ramp and stop on it; grounded remains `yes`.
- Cross both flat-to-ramp transitions without a visible hop.
- Push into the steep ramp; input cannot climb it directly.
- Confirm gravity moves the player down the steep ramp rather than parking.

## Steps

- Walk over `low_step` and up all four 0.3 m stairs.
- Descend the stairs without repeated airborne flicker.
- Push into `high_ledge`; it must reject the step.
- Test stairs beside a wall/corner and verify wall sliding remains stable.
- Add a temporary ceiling less than capsule height plus step height over a step;
  upward clearance must reject the step.

## Stability and modes

- Move against a wall continuously for one minute; no growing jitter or escape.
- Stand still for several minutes; no sinking or bouncing.
- Jump repeatedly with distinct key presses.
- Resize repeatedly and verify camera aspect and collision behavior are unchanged.
- Pause in a debugger, resume, and verify capped catch-up without tunneling.
- Compare low and high render rates; fixed-step travel distance must agree.
- Press N for noclip, fly through geometry, then return in open space.
- Toggle collision mode while overlapping geometry; recovery must succeed or the
  transition must return to the last valid position/refuse safely.
- Release/capture the mouse and repeat under Wayland when available.
