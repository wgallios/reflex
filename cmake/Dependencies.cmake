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

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.7
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Do not build tinygltf examples" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "Use tinygltf from one engine translation unit" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "Do not install the fetched tinygltf dependency" FORCE)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    recastnavigation
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
    GIT_TAG 6dc1667f580357e8a2154c28b7867bea7e8ad3a7 # v1.6.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "Do not build RecastDemo" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "Do not build Recast tests" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "Do not build Recast examples" FORCE)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d # 0.11.25
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

set(JSON_BuildTests OFF CACHE BOOL "Do not build nlohmann/json tests" FORCE)
set(JSON_Install OFF CACHE BOOL "Do not install nlohmann/json" FORCE)
FetchContent_MakeAvailable(SDL3 glad glm tinygltf stb nlohmann_json recastnavigation miniaudio)

# Generate only the OpenGL API this phase uses. REPRODUCIBLE makes GLAD use its
# bundled Khronos specification snapshot instead of downloading one at configure time.
include("${glad_SOURCE_DIR}/cmake/GladConfig.cmake")
set(GLAD_SOURCES_DIR "${glad_SOURCE_DIR}")
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3 EXTENSIONS NONE)
