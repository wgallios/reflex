# Reflex Engine contributor guidance

## Project direction

Reflex Engine is a small C++20, cross-platform 3D engine for first-person
shooters inspired by the 1999-2004 era. Linux is the primary development target,
but changes should remain portable to Windows.

The current implementation is Phase 2: an SDL3/OpenGL 3.3 Core platform shell
that loads static Blender-exported `.glb` environments and renders them with a
noclip camera. Preserve this working behavior when extending the engine.

## Build and validation

Use the repository's established CMake and Ninja workflow:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/reflex_engine
```

Run a custom scene with:

```bash
./build/reflex_engine assets/levels/my_scene.glb
```

Before handing off a change:

1. Configure and build the project.
2. Run CTest.
3. Inspect compiler warnings; engine code should compile cleanly with the
   configured warning flags.
4. Run `git diff --check`.
5. Exercise relevant runtime paths when a graphical display is available.
6. Run `git status --short` and report unrelated or untracked changes.

SDL's generated Wayland protocol sources may emit host XML-DTD validation
messages during a clean build. Do not confuse those messages with engine C/C++
compiler warnings.

## Architecture and ownership

- `Application` owns initialization, timing, the game loop, updates, rendering,
  and shutdown orchestration.
- `Window` exclusively owns the SDL window and OpenGL context and polls SDL
  events.
- `InputState` centralizes keyboard and relative-mouse input.
- `Camera` owns perspective and noclip camera state.
- `GltfLoader` parses glTF and uploads scene resources.
- `Renderer` draws loaded scene primitives using the static-mesh shader.
- `Shader`, `Mesh`, and `Texture` are move-only RAII wrappers for OpenGL objects.
- `Scene` owns GPU meshes and textures plus materials and draw instances.

OpenGL resources must be destroyed while the context is still current. In
particular, clear `Renderer` and `Scene` resources before calling
`Window::shutdown()`. Preserve cleanup after partial initialization failures.

Prefer small, focused classes and direct ownership. Do not introduce service
locators, dependency-injection containers, plugin systems, custom allocators,
render interfaces, or other speculative abstractions.

## Rendering and asset rules

- Require OpenGL 3.3 Core and load functions through GLAD.
- Keep GPU resource classes non-copyable and safely movable.
- Do not upload static mesh or texture data every frame.
- Keep imported glTF geometry in its right-handed, Y-up coordinate system; do
  not add arbitrary axis swaps to compensate for camera mistakes.
- Calculate node transforms as `parentWorld * local`.
- A node matrix overrides translation, rotation, and scale fields.
- glTF quaternion arrays are `x, y, z, w`; explicitly reorder them for GLM's
  `w, x, y, z` constructor.
- Account for accessor and buffer-view offsets, byte strides, normalized
  attributes, and declared component types.
- Reject unsupported required data clearly and skip unsupported primitives
  safely where possible.
- Do not vertically flip glTF images. The current upload convention preserves
  glTF UV/image semantics.
- Broken texture images should use the generated checkerboard fallback rather
  than crash the application.

Runtime shaders live in `assets/shaders/`. The default scene is
`assets/levels/test_scene.glb`. The build copies `assets/` beside the executable.
Regenerate the original test scene with:

```bash
python3 tools/generate_test_scene.py
```

Do not add third-party assets without clear licensing.

## Dependency policy

Dependencies are fetched in `cmake/Dependencies.cmake` and must remain pinned to
explicit stable tags or commit hashes. Do not use floating branches.

The tinygltf and stb implementation macros must remain defined in exactly one
translation unit, currently `src/scene/GltfLoader.cpp`. Never define
implementation macros in headers.

## Scope boundary

Phase 2 supports static textured GLB scenes and noclip exploration. Do not add
Phase 3 or gameplay features unless the user explicitly requests that phase.
Out-of-scope systems currently include:

- collision detection and trigger volumes;
- gravity, grounded movement, jumping, and stairs;
- rigid-body physics;
- weapons, enemies, and gameplay entities;
- scripting, save/load, an editor, or an ECS.

When Phase 3 is authorized, extend the existing architecture incrementally and
keep collision-aware movement separate from rendering and glTF parsing.
