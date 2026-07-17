# Reflex Engine

Reflex Engine is a small C++20, cross-platform 3D game engine for first-person
shooters inspired by the 1999-2004 era. Linux is the first target; SDL and CMake
keep the platform shell portable to Windows.

## Phase 5 scope

Phase 5 adds the first complete combat loop to the existing renderer, kinematic
controller, and interactive-world layer. The player can carry and switch between
a pistol, shotgun, and plasma launcher; collect named ammunition; reload; fire
finite hitscan traces or swept projectiles; damage simple hostile grunts; take
combat damage; die, respawn, and persist combat state in the quick-save slot.
Deterministic spread, fixed-step timing, placeholder presentation, bounded line
effects, hit feedback, and F7 diagnostics keep the prototype inspectable.

This remains deliberately smaller than an ECS, scripting system, or general
physics engine. Navigation meshes, skeletal animation, ragdolls, advanced AI,
audio playback, multiplayer, level transitions, scripting, and an editor are not
included.

## Dependencies

CMake downloads pinned source dependencies with `FetchContent`:

- SDL 3.4.10 (`release-3.4.10`)
- GLAD 2.0.8 (`v2.0.8`), generated for OpenGL 3.3 Core
- GLM 1.0.3 (`1.0.3`)
- tinygltf 2.9.7 (`v2.9.7`)
- stb at commit `31c1ad37456438565541f4919958214b6e762fb4`
- nlohmann/json 3.11.3 (`v3.11.3`) for validated save files

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
- `E`: interact with the targeted door or switch
- left mouse: fire the equipped weapon
- `R`: reload the equipped weapon
- `1` / `2` / `3`: select an owned weapon slot
- mouse wheel: cycle owned weapons
- `F4`: toggle gameplay bounds and entity inspection
- `F5`: write `saves/quicksave.json`
- `F6`: reset authored level/gameplay state
- `F7`: toggle combat traces, projectiles, enemy bounds/state, and RNG diagnostics
- `F9`: validate and load `saves/quicksave.json`
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
├── combat/
│   └── combat.json
├── levels/
│   └── test_scene.glb
└── shaders/
    ├── static_mesh.vert
    ├── static_mesh.frag
    ├── debug_line.vert / debug_line.frag
    └── hud.vert / hud.frag
```

The included test scene is original and generated locally. It contains a floor,
enclosing walls, doorway, low ceiling, stairs, two ledge heights, walkable and
steep ramps, corridor/corner tests, a fall platform, a spawn marker, and a
visual-only object excluded through glTF extras, five sliding doors, switches,
an automatic trigger door, a blue key, health/armor and combat pickups, three
grunt spawns, a damage floor, and two checkpoints. It retains embedded texture,
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

## Authoring Phase 4 gameplay

In Blender, select a mesh object or Empty, open **Object Properties -> Custom
Properties**, add the properties below, and enable **Include -> Custom
Properties** in the glTF exporter. Export with **File -> Export -> glTF 2.0 ->
glTF Binary (.glb)**. Mesh objects are appropriate for visible doors, switches,
and pickups. Empties are appropriate for triggers and checkpoints. Apply mesh
transforms with `Ctrl+A -> All Transforms` before export.

Every gameplay node needs `gameplay_type`. `entity_name` is optional when the
Blender object name is unique. IDs are the deterministic 64-bit FNV-1a hash of
that authored name; `entity_id` may provide an explicit positive number or
string identity. Duplicate IDs/names disable only the duplicate. Target links
use `target_name` and are resolved once after loading. The prefixes `door_`,
`switch_`, `trigger_`, `pickup_`, `damage_`, and `checkpoint_` are fallbacks,
but exported extras are authoritative.

All sizes are full extents in meters, not half extents. Runtime bounds are
axis-aligned. `collider_offset` is local to the authored node. Interactive
visuals retain the authored world matrix; a door applies a runtime translation
before that matrix. Door primitives therefore move with their dynamic collider.
Door and pickup meshes are omitted from the static triangle BVH.

### Supported metadata

Door (`gameplay_type = door`):

```text
collider_size       [x,y,z], required in practical authored levels
collider_offset     [x,y,z], default [0,0,0]
move_axis           [x,y,z], default [0,1,0]
move_distance       meters, default 2
open_speed          m/s, default 1.5
close_speed         m/s, default open_speed
auto_close_delay    seconds, 0 disables
starts_open         boolean
locked              boolean
required_key        string such as blue_key
toggle_mode         boolean, default true
```

Only sliding doors are supported. Motion clamps exactly to endpoints. Repeated
open/close events are idempotent. A closing door that would overlap the player
enters `Blocked`, waits 0.35 seconds, then reopens. Door collision is a simple
dynamic AABB collection iterated directly; the static BVH is never rebuilt.

Switch (`gameplay_type = switch`): `target_name`, `event` (`activate`,
`deactivate`, `toggle`, `open`, `close`, `lock`, or `unlock`), optional
`required_key`, `one_shot`, and `collider_size`. Trigger
(`gameplay_type = trigger`): `trigger_size`, `target_name`, `on_enter`,
`on_exit`, and `once`. Trigger overlap is player-capsule versus AABB and tracks
enter/stay/exit across fixed ticks; one-shot completion persists.

Pickup (`gameplay_type = pickup`):

```text
pickup_type   key | health | armor
item_id       required for a key, e.g. blue_key
amount        positive integer for health or armor
display_name  HUD text
trigger_size  overlap box
```

Pickups collect on overlap. Health/armor pickups remain when that vital is
already full. Keys are named strings and are not consumed when unlocking.
Collected pickups become inactive and remain absent after save/load.

Damage volume (`gameplay_type = damage_volume`): `trigger_size`,
`damage_per_second`, and `bypass_armor`. Damage uses a fractional accumulator at
the 120 Hz simulation rate. Armor absorbs 50 percent of eligible integer damage
until depleted. Health and armor default to 100/0 and clamp to 100.

Checkpoint (`gameplay_type = checkpoint`): `trigger_size`, `restore_health`, and
`restore_armor`. On first entry it records the current capsule-bottom position
and yaw. The authored `player_spawn` is the fallback checkpoint. Death disables
movement for two seconds, then restores checkpoint vitals, clears controller
velocity/transients, recovers overlap, and resumes without reloading the GLB.

### Gameplay update and persistence

At each 120 Hz tick Reflex Engine updates player movement, door motion and
dynamic colliders, trigger/pickup/hazard/checkpoint overlaps, then processes the
bounded FIFO event queue. Delayed events use simulation delivery time plus a
monotonic sequence number. A maximum of 256 immediate events is processed per
tick, preventing recursive event storms.

F5 writes one human-readable slot at `saves/quicksave.json`. Format version 2
stores the level path, player transform/view, vitals, key inventory, checkpoint,
and persistent state keyed by authored entity name. F9 parses and validates a
temporary save first, rejects future versions and other levels, resets authored
state, applies known entities, ignores state for removed entities, restores the
player, and clears pending events. If overlap recovery fails, the previous live
state is restored. F6 performs the same authored reset without reparsing the
GLB.

The screen-space HUD uses a built-in code-defined 5x7 bitmap alphabet (no
external font asset). Bottom-left shows health, armor, and keys; the center ray
shows an interaction prompt; priority-aware temporary messages appear at the
top. F4 draws gameplay bounds and reports the targeted entity, entity/event
counts, and current FPS/noclip mode.

See [the Phase 4 manual test matrix](docs/phase4-test-matrix.md) for the complete
interaction, door, key, pickup, death, checkpoint, and persistence scenarios.

## Phase 5 combat

Combat runs after player/gameplay updates at the existing fixed 120 Hz rate:
weapon transitions and fire requests, hitscan/projectile resolution, queued
damage, enemy perception/movement/attacks, pickup collection, then transient
presentation updates. Rendering never controls fire rate, reload timing,
projectile speed, enemy cooldowns, or damage.

### Weapons and ammunition

Reusable definitions live in `assets/combat/combat.json`. `maximum_ammo` maps
arbitrary ammo IDs to carry limits. Every weapon requires `id`, `display_name`,
`attack_type` (`hitscan` or `projectile`), positive `damage`, `shots_per_second`,
`ammo_type`, `magazine_size`, and `reload_time`. Optional fields include `range`,
`equip_time`, `spread_degrees`, `pellets`, `automatic`, `tracer_frequency`, and
the projectile fields `projectile_speed`, `projectile_radius`,
`projectile_lifetime`, `splash_damage`, `splash_radius`, and `self_damage`.
Definitions are validated atomically; duplicate IDs, unknown ammo IDs, and
non-positive required values fail combat initialization clearly.

- Pistol: 12-round bullet magazine, 18 damage, low-spread 120 m hitscan.
- Shotgun: six shells, eight 10-damage pellets, 6.5-degree spread, 45 m range.
- Plasma launcher: four rounds, visible 22 m/s swept spheres, 45 direct damage,
  and up to 55 splash damage over 4 m.

Weapons receive a full magazine when first acquired. Extra `ammo_grant` goes to
reserve; a duplicate weapon grants that reserve amount. Whole-magazine reloads
transfer only missing rounds. Switching cancels reload by holstering the old
instance. Save/load restores each magazine but normalizes weapons to `Ready` or
`Empty`; active projectiles and presentation effects are not saved.

Spread uses a dedicated seeded combat RNG and uniform disk sampling projected
into a cone. Hitscan chooses the nearest static triangle, dynamic collider, or
enemy AABB. Projectiles use a swept sphere against the static BVH and dynamic
colliders, deactivate on the first impact, and expire after a finite lifetime.
Splash damage falls off linearly to zero; static world and dynamic door geometry
block it. Plasma self-damage is enabled, but rocket jumping is not tuned.

The first-person weapon is an original code-generated screen-space placeholder.
Valid shots produce a short muzzle flash and bounded line tracers; projectiles
and explosions use bounded line presentation. Flesh hits and kills alter the
crosshair. Surface-aligned line-cross marks distinguish stone, metal, and flesh
impacts and share the bounded effect cap. There is no audio system, particle
sprite system, or textured decal renderer; effects remain generated placeholders.

### Enemies and Blender metadata

`combat.json` defines the `grunt`: 75 health, 2.5 m/s direct pursuit, 25 m sight,
a 100-degree horizontal field of view, 0.25-second reaction delay, and an
inaccurate fixed-rate hitscan attack. Its explicit states are `Idle`, `Alert`,
`Chasing`, `Attacking`, `Pain`, and `Dead`. Perception requires range, cone, and
static/dynamic line of sight. Lost targets are pursued briefly. Movement uses a
three-iteration upright-capsule slide against static geometry, doors, and enemy
AABBs. It works in open rooms/corridors but cannot route through complex mazes.

Create an Empty or placeholder mesh in Blender and export custom properties:

```text
gameplay_type = enemy_spawn
entity_name = enemy_grunt_01
enemy_type = grunt
starts_active = true
collider_size = [0.8, 1.8, 0.8]
```

The node transform supplies spawn position and initial facing. A placeholder
mesh moves with the actor; a dead grunt is hidden and stops blocking.
`starts_active = false` allows existing `Activate`/`Deactivate` events to control
the encounter. Grunts have no skeletal animation, hearing, cover selection,
squad behavior, or navmesh navigation.

Combat pickups extend existing `gameplay_type = pickup` metadata:

```text
pickup_type = weapon   weapon_id = shotgun   ammo_grant = 8
pickup_type = ammo     ammo_type = shells    amount = 8
display_name = Shotgun Shells
trigger_size = [0.8, 0.8, 0.8]
```

Weapon pickups grant ownership; duplicate weapons grant reserve ammo. Ammo
pickups remain when the pool is full. Collection persists by authored entity
name. Damage uses a bounded deterministic FIFO queue. Player armor retains the
Phase 4 rule of absorbing 50 percent until depleted. Pain interrupts attacks;
enemy death clamps at zero and occurs exactly once.

The HUD adds weapon name, loaded/reserve ammo, reload color, the generated
viewmodel, hit/kill feedback, and directional damage text. F7 shows traces,
projectiles, enemy bounds/state/health, active counts, and RNG seed/sequence.

Save format 2 adds owned weapons, equipped weapon, magazines, reserve ammo, and
enemy name/health/position/active state. Loading validates known IDs, ammo and
magazine bounds, finite positions, and authored enemy identities before mutation.
Living enemies resume in stable `Idle`; active projectiles, cooldowns, recoil,
effects, and mid-animation states are discarded.

See [the Phase 5 manual test matrix](docs/phase5-test-matrix.md) for weapon,
projectile, perception, AI, effects, pickup, persistence, and timing checks.

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

Known limitations are static triangle collision plus direct-iteration dynamic
AABBs for doors, a five-sphere capsule approximation, and no moving platforms,
dynamic bodies, crouching, ladders, swimming, elevators, arbitrary capsule
orientation, network prediction, or exact Quake movement. Collision does not
currently distinguish material/surface types.

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

### Custom properties or gameplay entities are missing

Enable **Custom Properties** in Blender's glTF export options and use the exact
lowercase `gameplay_type` values documented above. Startup prints the parsed
entity count and warns about unknown types, missing required pickup data,
invalid dimensions/door axes, duplicate names, and unresolved switch/trigger
targets. One bad optional entity does not reject the visual level.

### Door visual/collision mismatch or player passes through a door

Give the door a positive full-extents `collider_size`, apply transforms, and
keep it as one gameplay node. F4 shows the runtime AABB. Door meshes are removed
from static collision automatically; adding `collision = true` does not replace
the dynamic box. Very thin boxes should still have a sensible thickness such as
0.2 m. Rotating doors are not supported.

### Interaction works through a wall or no prompt appears

Interaction range is 2.75 m from the camera. Ensure the entity has a correctly
sized box around its visible object. Static triangles and other dynamic doors
occlude the ray. Use F4 to compare the target box with the center crosshair; a
wall excluded from collision cannot occlude interaction.

### Trigger repeats, pickup does not collect, or key is not recognized

Triggers fire enter only on the outside-to-inside transition; use `once = true`
for persistent one-shot behavior. Check `trigger_size` uses full extents and
encloses the player's capsule. Health/armor pickups deliberately remain at the
maximum vital. A locked door's `required_key` must exactly match the key
pickup's case-sensitive `item_id`.

### Save file fails to load

Read the console diagnostic. F9 rejects malformed JSON, any `format_version`
other than 2, non-finite/invalid player or combat fields, and saves whose recorded level
path differs from the current level. Delete `saves/quicksave.json` to start a
fresh slot. Unknown saved entity names are harmless and ignored.

### Respawn is embedded or HUD text is absent

Place checkpoints in capsule-clear space and make their trigger high enough to
overlap the player. Respawn uses bounded penetration recovery and falls back to
`player_spawn` when needed. For HUD failures, confirm `hud.vert` and `hud.frag`
were copied with the other runtime shaders and inspect shader compilation logs.

### Gameplay differs with render rate or Wayland mouse capture fails

Gameplay, doors, damage, and trigger transitions run only on the 120 Hz fixed
simulation. Presentation-time HUD message fading does not affect state. If
relative mode is denied under a compositor, try an X11 session or ensure the
Wayland pointer-constraints protocols are available; the SDL warning is
nonfatal and Escape still releases a successful capture.

### Mouse capture under Wayland

Click the window before moving the mouse. Some compositors restrict pointer lock
or synthetic mouse events. Try an X11 session or diagnose with
`SDL_VIDEODRIVER=x11 ./build/reflex_engine`; also ensure the SDL Wayland build
packages above were installed before configuring.

### Unsupported accessor type

Re-export using standard Blender glTF settings. Sparse accessors and unusual
extension-compressed data are not supported. Index components must be unsigned
8-, 16-, or 32-bit values; attributes must have the expected vector width.

### Weapon will not fire, fires too quickly, or will not reload

The window must be captured, the weapon must be `Ready`, and its magazine must
contain ammunition. Semi-automatic weapons require a new press. Reload requires
both magazine space and reserve ammunition. Use F7 to inspect state/timers and
verify positive `shots_per_second`, `magazine_size`, and `reload_time` values in
`combat.json`; all timing is fixed-step.

### Hitscan, shotgun spread, projectile, or splash problems

F7 draws traces and projectiles. Nearest static/dynamic/enemy hits win. A wall
excluded from collision cannot block combat. Spread is deterministic uniform-disk
cone sampling, so a small sample need not look symmetrical. Projectiles use swept
spheres; very malformed triangles can still produce poor contacts. Static world
and dynamic doors block splash LOS, and active projectiles are intentionally
discarded by quick load.

### Enemy cannot see, sees through walls, becomes stuck, or attacks too fast

Confirm `enemy_type` exists, the spawn faces the intended direction, and
`starts_active` is true or an Activate event was delivered. Sight needs range,
horizontal FOV, and both static/dynamic LOS. F7 shows state and facing. Direct
pursuit only slides locally, so complex corners or mazes require simpler room
layout; no navmesh exists. Attack cadence uses `shots_per_second` at 120 Hz.

### Enemy damage or combat effects are missing

Check that the enemy box overlaps the visible placeholder and that no nearer wall
or door blocks the trace. F7 shows actor health/state and the last traces. Muzzle,
tracer, projectile, explosion, hit-marker, and viewmodel feedback are generated
placeholders. There is no audio, textured particle, or persistent decal backend.

### Missing OpenGL packages or unsupported OpenGL version

Install `libgl1-mesa-dev` and an appropriate Mesa or vendor driver. Reflex Engine
requires OpenGL 3.3 Core and exits nonzero if GLAD cannot load it. `glxinfo -B`
or `eglinfo` can show the driver version. A remote/headless shell also needs a
working `DISPLAY` or `WAYLAND_DISPLAY`.

## Recommended Phase 6

A sensible Phase 6 is focused presentation and encounter authoring: a small audio
layer, licensed sprite/model assets, simple frame animation, surface-authored
impact effects, and optional authored waypoint navigation. Advanced AI, campaign
systems, multiplayer, and editor work should remain separately scoped.
