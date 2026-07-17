# Reflex Engine

Reflex Engine is a small C++20, cross-platform 3D game engine for first-person
shooters inspired by the 1999-2004 era. Linux is the first target; SDL and CMake
keep the platform shell portable to Windows.

## Phase 2 scope

Phase 2 preserves the Phase 1 SDL3/OpenGL shell and adds a static glTF scene
renderer and freely movable noclip camera. It includes file-based GLSL shaders,
move-only OpenGL mesh/texture/program resources, glTF node traversal, basic
base-color materials, embedded PNG/JPEG textures, generated normals, a fixed
directional light, and centralized SDL input.

It does not include collision, gravity, grounded movement, gameplay, weapons,
enemies, physics, scripting, an ECS, or an editor.

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
- `Space` / `Left Ctrl`: move up / down
- mouse: yaw and pitch
- `Left Shift`: sprint
- `Escape`: release the pointer

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

The included 3 KB test scene is original, generated locally, and exercises an
embedded PNG, an untextured base-color material, interleaved vertex attributes,
8- and 16-bit index accessors, and a nested child transform. Regenerate it with:

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
texture or mesh extensions are outside Phase 2. Unsupported primitive modes are
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

## Planned Phase 3

The recommended next phase is collision-aware first-person movement against the
loaded static world, with gravity, grounded state, jumping, and stair handling.
Those systems are intentionally not part of Phase 2.
