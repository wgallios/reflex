# Reflex Engine

Reflex Engine is a small C++20, cross-platform 3D game engine for first-person
shooters inspired by the 1999-2004 era. Linux is the first target; SDL and CMake
keep the platform shell portable to Windows.

## Phase 3 scope

Phase 3 adds a purpose-built kinematic FPS controller to the existing SDL3 and
OpenGL platform/rendering shell. Static triangles are extracted from the loaded
GLB once, transformed into world space, and indexed by a deterministic BVH. The
player uses a vertical capsule approximation with swept collision, iterative
plane sliding, gravity, jumping, walkable-slope classification, ground snapping,
step handling, limited penetration recovery, noclip switching, and debug lines.

This remains deliberately smaller than a general physics engine. Collision is
static only. Weapons, enemies, damage, pickups, triggers, moving platforms,
rigid bodies, scripting, an ECS, and an editor are not included.

## Dependencies

CMake downloads pinned source dependencies with `FetchContent`:

- SDL 3.4.10 (`release-3.4.10`)
- GLAD 2.0.8 (`v2.0.8`), generated for OpenGL 3.3 Core
- GLM 1.0.3 (`1.0.3`)
- tinygltf 2.9.7 (`v2.9.7`)
- stb at commit `31c1ad37456438565541f4919958214b6e762fb4`

`GltfLoader.cpp` is the only translation unit that defines
`TINYGLTF_IMPLEMENTATION`, `STB_IMAGE_IMPLEMENTATION`, and
`STB_IMAGE_WRITE_IMPLEMENTATION`. The separately pinned stb headers are used by
that implementation rather than relying on an implicit system copy.

## Ubuntu prerequisites

This focused package set enables SDL's OpenGL, X11, and Wayland paths on current
Ubuntu releases:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    ninja-build \
    pkg-config \
    python3 \
    python3-jinja2 \
    libgl1-mesa-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libdecor-0-dev
```

SDL loads many desktop integrations dynamically. Audio, controller, and other
SDL development packages are optional because Reflex Engine initializes only
video in this phase. XScreenSaver integration remains disabled, so `libxss-dev`
is not required. Python and Jinja are used by GLAD generation; Python also runs
the optional test-scene generator.

## Build and run

The first configure needs internet access to fetch dependencies.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/reflex_engine
```

The default is `assets/levels/test_scene.glb`. Pass another binary glTF scene as
the first argument:

```bash
./build/reflex_engine assets/levels/my_room.glb
```

Runtime assets are also copied beside the executable after each build, allowing
the program to be launched from inside `build/`. A missing or invalid scene is
treated as an initialization failure: the engine reports it and exits nonzero.

## Controls

Click the window to capture the pointer. While captured:

- `W` / `S`: forward / backward
- `A` / `D`: strafe left / right
- `Space`: jump while grounded
- mouse: yaw and pitch
- `Left Shift`: sprint
- `N`: toggle collision-aware FPS movement and noclip
- `F3`: toggle collision lines and rate-limited diagnostics
- `Escape`: release the pointer

In noclip mode, `Space` and `Left Ctrl` move up and down. Switching back to
collision mode runs penetration recovery; if the current location is invalid,
the controller returns to its last valid collision-mode position or refuses the
transition.

With the pointer already released, press `Escape` again to quit. Closing the
window always quits. Losing window focus releases the pointer and clears held
input so movement cannot become stuck.

## Asset layout

```text
assets/
├── levels/
│   └── test_scene.glb
└── shaders/
    ├── static_mesh.vert
    └── static_mesh.frag
```

The included test scene is original and generated locally. It contains a floor,
enclosing walls, doorway, low ceiling, stairs, two ledge heights, walkable and
steep ramps, corridor/corner tests, a fall platform, a spawn marker, and a
visual-only object excluded through glTF extras. It retains embedded texture,
interleaved-attribute, and 8-/16-bit index coverage. Regenerate it with:

```bash
python3 tools/generate_test_scene.py
```

## Exporting a Blender environment

1. Create a simple room or environment.
2. UV unwrap geometry that uses image textures.
3. Connect each image texture to the Principled BSDF **Base Color** input.
4. Select the exported objects and apply `Ctrl+A -> All Transforms`.
5. Choose `File -> Export -> glTF 2.0`.
6. Set `Format -> glTF Binary (.glb)`.
7. Enable selected objects when appropriate, materials, UVs, and normals.
8. Keep Blender's normal glTF Y-up export behavior and embed images in the GLB.

Blender procedural materials must be baked to image textures if their appearance
is expected in Reflex Engine. Place the exported file under `assets/levels/` and
pass its path on the command line.

### Phase 3 collision test level

Use one engine unit as one meter. Build a large floor, enclosing walls, a
doorway, low ceiling, several stairs no taller than 0.35 m each, a shallow ramp,
a ramp steeper than 46 degrees, ledges below and above 0.35 m, a narrow corridor,
inside/outside corners, and an elevated fall platform. Prefer simple manifold
meshes. Avoid concave n-gons for collision-critical surfaces and triangulate
them before export when the result is uncertain.

Create the spawn marker with `Add -> Empty`, name it `player_spawn`, place it a
few centimeters above the floor, and rotate it about Blender's vertical axis to
set view yaw. Applying `Ctrl+A -> All Transforms` to mesh objects before export
avoids displaced visual/collision geometry. Blender custom properties may also
mark the Empty with `type = player_spawn` when custom properties are exported to
glTF extras.

All triangle primitives collide by default. A node is visual-only when its name
starts with `nocollide_` or its glTF extras contain `"collision": false`.
The extras rule takes precedence for normally named objects.

## Player and collision design

The world is right-handed and Y-up, and `1 engine unit = 1 meter`. Player
position means the bottom point of an upright capsule. The camera position is
that point plus the eye-height offset. Collision movement uses camera yaw only;
pitch never tilts the capsule or changes grounded walking direction.

Defaults are centralized in `PlayerSettings`:

| Setting | Default |
|---|---:|
| Capsule height / radius | 1.8 m / 0.35 m |
| Eye height | 1.65 m |
| Walk / sprint speed | 5.0 m/s / 8.0 m/s |
| Ground / air acceleration | 35 / 8 m/s² |
| Jump speed | 6.0 m/s |
| Gravity / terminal velocity | -15 / -45 m/s² |
| Maximum slope | 46 degrees |
| Maximum step | 0.35 m |
| Step landing search | 0.2 m |
| Ground probe / snap | 0.08 m / 0.18 m |
| Slide / penetration iterations | 6 / 6 |

Rendering is variable-rate, but player simulation runs from a capped accumulator
at 120 Hz. A long frame contributes at most 0.25 seconds, preventing a debugger
pause from creating an unbounded catch-up loop. Jump presses remain latched until
a fixed step consumes them.

The broad phase is a median-split AABB BVH with eight or fewer triangles per
leaf. The upright capsule is represented by a vertical segment sampled by five
overlapping swept spheres. Each sphere uses conservative advancement against
closest-point-on-triangle distance, which prevents ordinary-speed tunneling.
After the earliest hit, movement advances to a contact offset and projects the
remainder onto one plane, a two-plane crease, or stops at a three-plane corner.
Coplanar triangle seams are filtered when an adjacent triangle provides
non-blocking support.

Ground probes accept only normals satisfying
`dot(normal, up) >= cos(maximumSlopeAngle)`. Walkable slopes retain tangent
movement and ground snap; steep slopes remain ungrounded so gravity produces a
downhill slide. A blocked step performs up, horizontal, and walkable-down sweeps
and is accepted only with clearance and a valid landing. Penetration correction
is bounded and is a recovery path, not the normal movement algorithm.

F3 draws the capsule, probe, contact points, and normals. Twice per second it
prints mode, grounded state, player position, velocity, candidate triangles,
narrow-phase tests, contacts, slide iterations, and step attempts/successes.
See [the Phase 3 manual test matrix](docs/phase3-test-matrix.md) for repeatable
runtime checks.

## Supported collision behavior

- static indexed triangle primitives with full node world transforms
- degenerate-triangle rejection and per-triangle bounds/normals
- BVH broad-phase overlap queries and ray/triangle queries
- swept upright capsule approximation against floors, walls, ceilings, ramps,
  stairs, ledges, and simple obstacles
- multiple-plane wall/corner sliding, gravity, terminal velocity, jumping,
  ground probing/snapping, slope rejection/sliding, and bounded recovery
- spawn by `extras.type = player_spawn`, exact `player_spawn` node name, or the
  documented fallback `(0, 1, 5)`
- safe fallback to noclip when a scene contains no collision triangles

Known limitations are static collision only, five-sphere capsule approximation,
no moving platforms/doors, dynamic bodies, crouching, ladders, swimming,
elevators, triggers, arbitrary capsule orientation, network prediction, or exact
Quake movement. Collision does not currently distinguish material/surface types.

## Supported glTF features

- binary glTF 2.0 `.glb` files and embedded binary buffers
- default scene selection, or the first scene when no default is marked
- recursive node hierarchy with parent-child transform inheritance
- node matrices or glTF-order translation, rotation, and scale composition
- shared mesh primitives instanced by multiple nodes
- indexed triangles with unsigned byte, unsigned short, or unsigned int indices
- interleaved/strided attributes with accessor and buffer-view byte offsets
- positions, normals, and `TEXCOORD_0`, including normalized integer attributes
- generated area-weighted normals when normals are absent
- `(0, 0)` UV fallback when UVs are absent
- base-color factor, base-color texture, and double-sided materials
- embedded 8-bit PNG and JPEG images decoded by tinygltf/stb
- glTF wrapping/filter settings where valid, mipmap generation, and a magenta
  checkerboard fallback for unusable texture images

Texture pixels are uploaded in decoder row order; stb's vertical-flip option is
not enabled. This deliberately matches glTF UV/image semantics without modifying
imported UV coordinates.

## Unsupported glTF features

Sparse accessors, non-triangle primitive modes, animation, skins, morph targets,
cameras/lights, external `.gltf` files, multiple UV sets, vertex colors, metallic-
roughness shading, normal/occlusion/emissive maps, alpha modes, and compressed
texture or mesh extensions are outside the current scope. Unsupported primitive modes are
warned about and skipped. Invalid required geometry/accessors are skipped safely;
16-bit-per-channel images use the checkerboard fallback. If the overall scene
cannot initialize, the process exits nonzero.

## Coordinate conventions

Imported geometry is left in glTF's right-handed, positive-Y-up coordinate
system, where positive Z is the asset-forward basis. Node matrices remain
column-major, and world transforms are calculated as
`parentWorld * local`. glTF quaternion arrays are read as `x, y, z, w` and
explicitly reordered for GLM.

The OpenGL camera is right-handed and initially looks down negative Z, with Y as
up. No imported axis is swapped or negated; build and inspect scenes using the
normal Blender glTF export conversion.

## Troubleshooting

### Scene file not found or invalid

Paths are resolved first from the working directory, then beside the executable.
Pass an explicit path and check capitalization. Only `.glb` is accepted. An
actionable tinygltf error is printed before the engine exits nonzero.

### Shader file not found

Keep `assets/shaders/static_mesh.vert` and `static_mesh.frag` present. Rebuilding
copies them beside the executable. If launching a copied executable elsewhere,
copy its `assets` directory too.

### Black geometry

Export normals or allow the loader to generate them. Check face winding and make
the Blender material double-sided if the environment intentionally exposes back
faces. Confirm transforms have reasonable scales and that the camera is not
inside a closed mesh.

### Missing textures

Use PNG or JPEG image textures connected to Principled BSDF Base Color and embed
them in the GLB. Procedural node materials are not images and must be baked. A
magenta checkerboard means the referenced image was missing or could not decode.

### Inverted or incorrect transforms

Apply `Ctrl+A -> All Transforms` before export, retain Blender's default glTF
Y-up handling, and do not manually rotate the exported world to compensate for
the camera. Nested transforms use `parent * local`; malformed cyclic node graphs
are rejected.

### Player falls through the floor

Confirm the floor is an exported triangle mesh, the node is not named with the
`nocollide_` prefix, and it does not have `collision = false`. Apply transforms,
triangulate suspect faces, and use F3 to confirm candidate/test counts are
nonzero. Extremely thin or malformed geometry should be replaced with clean
triangles; the controller is tuned for normal gameplay speeds at 120 Hz.

### Player spawns inside geometry

Move `player_spawn` so its origin marks the capsule bottom a few centimeters
above a clear floor area. The capsule needs 1.8 m vertical and 0.7 m horizontal
clearance. Startup reports bounded recovery failure and falls back to noclip if
the overlap is too deep.

### Cannot climb stairs or excessive wall jitter

Keep each riser at or below the configured 0.35 m step height and provide at
least capsule-height overhead clearance. Apply transforms and avoid duplicate or
nearly coincident faces. F3 reports step attempts and slide iterations;
persistent iteration limits usually indicate overlapping collision surfaces or
a scale/unit mismatch.

### Incorrect slope behavior

The walkable limit is 46 degrees and uses world-space triangle normals. Apply
node transforms, triangulate the ramp consistently, and verify Blender exports
Y-up normally. Steeper surfaces intentionally do not ground the player and
gravity moves the player downhill.

### Camera is too high or low

Player position is the capsule bottom and eye height defaults to 1.65 m. Adjust
`PlayerSettings::eyeHeight` without moving imported geometry; it must remain
between zero and the 1.8 m capsule height.

### Collision geometry is displaced or absent

Visual and collision instances share `parentWorld * local`, so a mismatch often
means unapplied Blender transforms or an unexpected parent. Check startup's
collision triangle count/world bounds and verify exclusion names/extras.

### Movement varies with frame rate

The application simulates at 120 Hz. Do not call the controller from the render
path or change speed by render delta. If a local modification causes drift,
compare fixed-duration travel with VSync on/off and run `collision_math` through
CTest.

### Mouse capture under Wayland

Click the window before moving the mouse. Some compositors restrict pointer lock
or synthetic mouse events. Try an X11 session or diagnose with
`SDL_VIDEODRIVER=x11 ./build/reflex_engine`; also ensure the SDL Wayland build
packages above were installed before configuring.

### Unsupported accessor type

Re-export using standard Blender glTF settings. Sparse accessors and unusual
extension-compressed data are not supported. Index components must be unsigned
8-, 16-, or 32-bit values; attributes must have the expected vector width.

### Missing OpenGL packages or unsupported OpenGL version

Install `libgl1-mesa-dev` and an appropriate Mesa or vendor driver. Reflex Engine
requires OpenGL 3.3 Core and exits nonzero if GLAD cannot load it. `glxinfo -B`
or `eglinfo` can show the driver version. A remote/headless shell also needs a
working `DISPLAY` or `WAYLAND_DISPLAY`.

## Recommended Phase 4

A sensible next phase is a deliberately narrow gameplay foundation: authored
static interaction metadata, a minimal non-ECS game-object boundary, and one
testable interaction loop. Combat, AI, doors, triggers, rigid bodies, and editor
work should each remain separately scoped rather than being folded into the
collision controller.
