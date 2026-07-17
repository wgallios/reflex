include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "Build SDL as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build SDL as a static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL examples" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Do not install the fetched SDL dependency" FORCE)
# Screen-saver inhibition is not needed by this phase and otherwise adds a
# mandatory libxss-dev configure-time dependency to SDL's X11 backend.
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "Disable optional XScreenSaver integration" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL3 glad glm)

# Generate only the OpenGL API this phase uses. REPRODUCIBLE makes GLAD use its
# bundled Khronos specification snapshot instead of downloading one at configure time.
include("${glad_SOURCE_DIR}/cmake/GladConfig.cmake")
set(GLAD_SOURCES_DIR "${glad_SOURCE_DIR}")
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3 EXTENSIONS NONE)
