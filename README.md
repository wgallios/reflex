# Reflex Engine

Reflex Engine is a small C++20, cross-platform 3D game engine for building
first-person shooters inspired by the 1999-2004 era. Linux is the first target,
while the CMake and SDL platform layer remain suitable for Windows.

## Phase 1 scope

Phase 1 provides only the platform and rendering shell:

- an SDL3 resizable, high-pixel-density window;
- an OpenGL 3.3 Core context with a 24-bit depth buffer and double buffering;
- OpenGL function loading through GLAD;
- a steady-clock game loop with a clamped frame delta;
- Escape and window-close handling;
- framebuffer resize tracking and viewport updates;
- color/depth clearing, depth testing, buffer swapping, and orderly shutdown.

It intentionally has no model or world loading, collision, physics, gameplay,
entity-component system, shader abstraction, or asset management.

## Dependencies

CMake downloads source dependencies with `FetchContent` at configure time. They
are pinned to stable tags:

- SDL 3.4.10 (`release-3.4.10`)
- GLAD 2.0.8 (`v2.0.8`)
- GLM 1.0.3 (`1.0.3`)

GLAD is generated at configure time for only OpenGL 3.3 Core. Its reproducible
mode uses the Khronos specification snapshot bundled with the pinned GLAD source.
No generated loader needs to be copied into `external/` manually.

## Ubuntu prerequisites

The following is the focused package set for building this project with SDL's
X11 and Wayland video backends on a current Ubuntu release:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    ninja-build \
    pkg-config \
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

SDL can build with fewer features, and it loads most Linux desktop integrations
dynamically. Its upstream Linux documentation lists additional packages for
audio, controllers, input methods, and other SDL subsystems; Phase 1 initializes
only the video subsystem, so those are optional here. The project explicitly
disables SDL's optional XScreenSaver integration, avoiding an otherwise required
`libxss-dev`; this does not disable the X11 backend. `python3-jinja2` supports
GLAD code generation.

## Build and run

An internet connection is required the first time CMake fetches dependencies.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/reflex_engine
```

The application prints SDL initialization and OpenGL vendor, renderer, and
version information. It opens a 1280x720 resizable window with a dark blue-gray
background. Resize the window to exercise viewport updates. Press Escape or use
the window close button to exit.

## Troubleshooting

### Missing OpenGL development packages

If CMake cannot find OpenGL headers or libraries, install `libgl1-mesa-dev`,
delete the incomplete `build/` directory, and configure again. Confirm that a
working GPU driver or Mesa software renderer is installed as appropriate for the
machine.

### Wayland or X11 issues

Install the X11 and Wayland development packages shown above before configuring,
so SDL builds both backends. At runtime, force a backend for diagnosis with
`SDL_VIDEODRIVER=x11 ./build/reflex_engine` or
`SDL_VIDEODRIVER=wayland ./build/reflex_engine`. A remote or headless shell
also needs access to a display server; check `DISPLAY` and `WAYLAND_DISPLAY`.

### GLAD loading failure

`Failed to load OpenGL functions through GLAD` means SDL created a context but
the driver did not provide the required entry points. Verify the graphics driver,
avoid running through a display connection without GL support, and inspect the
preceding SDL error. Reconfigure from a clean build directory if GLAD generation
itself failed.

### Unsupported OpenGL version

The engine requires OpenGL 3.3 Core. Update the GPU driver or use hardware that
supports it. On Mesa, `glxinfo -B` or `eglinfo` can show the available version.
The application exits with a nonzero status instead of attempting an older
compatibility context.

## Planned Phase 2

Phase 2 will add shader compilation/linking, mesh vertex/index buffers, a
perspective camera, and loading a Blender-exported `.glb` world. Those features
are deliberately outside this phase.
