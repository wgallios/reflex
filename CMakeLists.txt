cmake_minimum_required(VERSION 3.24)

project(ReflexEngine
    VERSION 0.6.0
    DESCRIPTION "A 1999-2004-inspired first-person shooter engine"
    LANGUAGES C CXX
)

function(reflex_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

add_library(reflex_phase6_core STATIC
    src/animation/Animation.cpp
    src/navigation/NavigationSystem.cpp
    src/campaign/Campaign.cpp
    src/audio/AudioSystem.cpp
    src/profiling/Profiler.cpp
    src/assets/AssetManifest.cpp
)
target_include_directories(reflex_phase6_core PUBLIC src)
target_include_directories(reflex_phase6_core SYSTEM PRIVATE "${miniaudio_SOURCE_DIR}")
target_link_libraries(reflex_phase6_core PUBLIC
    glm::glm nlohmann_json::nlohmann_json Recast Detour
)
reflex_warnings(reflex_phase6_core)

add_executable(reflex_engine
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/input/InputState.cpp
    src/collision/CollisionQueries.cpp
    src/collision/CollisionWorld.cpp
    src/collision/DynamicCollisionWorld.cpp
    src/combat/CombatTypes.cpp
    src/combat/CombatSystem.cpp
    src/gameplay/PlayerController.cpp
    src/gameplay/PlayerState.cpp
    src/gameplay/GameplayWorld.cpp
    src/persistence/SaveGame.cpp
    src/debug/DebugDraw.cpp
    src/rendering/Mesh.cpp
    src/rendering/SkinnedMesh.cpp
    src/rendering/HudRenderer.cpp
    src/rendering/Renderer.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp
    src/scene/Camera.cpp
    src/scene/Scene.cpp
    src/scene/GltfLoader.cpp
    src/scene/TinyGltfImplementation.cpp
)

target_include_directories(reflex_engine PRIVATE src)
target_include_directories(reflex_engine SYSTEM PRIVATE
    "${stb_SOURCE_DIR}"
    "${tinygltf_SOURCE_DIR}"
)

target_link_libraries(reflex_engine PRIVATE
    SDL3::SDL3
    glad_gl_core_33
    glm::glm
    nlohmann_json::nlohmann_json
    reflex_phase6_core
)

reflex_warnings(reflex_engine)

include(CTest)
if(BUILD_TESTING)
    add_executable(reflex_engine_camera_tests
        tests/CameraTests.cpp
        src/input/InputState.cpp
        src/scene/Camera.cpp
    )
    target_include_directories(reflex_engine_camera_tests PRIVATE src)
    target_link_libraries(reflex_engine_camera_tests PRIVATE SDL3::SDL3 glm::glm)
    reflex_warnings(reflex_engine_camera_tests)
    add_test(NAME camera_input COMMAND reflex_engine_camera_tests)

    add_executable(reflex_engine_collision_tests
        tests/CollisionTests.cpp
        src/collision/CollisionQueries.cpp
        src/collision/CollisionWorld.cpp
        src/collision/DynamicCollisionWorld.cpp
        src/gameplay/PlayerController.cpp
    )
    target_include_directories(reflex_engine_collision_tests PRIVATE src)
    target_link_libraries(reflex_engine_collision_tests PRIVATE glm::glm)
    reflex_warnings(reflex_engine_collision_tests)
    add_test(NAME collision_math COMMAND reflex_engine_collision_tests)

    add_executable(reflex_engine_gameplay_tests
        tests/GameplayTests.cpp
        src/combat/CombatTypes.cpp
        src/combat/CombatSystem.cpp
        src/collision/CollisionQueries.cpp
        src/collision/CollisionWorld.cpp
        src/collision/DynamicCollisionWorld.cpp
        src/gameplay/PlayerState.cpp
        src/gameplay/GameplayWorld.cpp
        src/persistence/SaveGame.cpp
        src/rendering/Mesh.cpp
        src/rendering/SkinnedMesh.cpp
        src/rendering/Texture.cpp
        src/scene/Scene.cpp
    )
    target_include_directories(reflex_engine_gameplay_tests PRIVATE src)
    target_link_libraries(reflex_engine_gameplay_tests PRIVATE glm::glm glad_gl_core_33 nlohmann_json::nlohmann_json reflex_phase6_core)
    reflex_warnings(reflex_engine_gameplay_tests)
    add_test(NAME gameplay_persistence COMMAND reflex_engine_gameplay_tests)

    add_executable(reflex_engine_combat_tests
        tests/CombatTests.cpp
        src/combat/CombatTypes.cpp
        src/combat/CombatSystem.cpp
        src/collision/CollisionQueries.cpp
        src/collision/CollisionWorld.cpp
        src/collision/DynamicCollisionWorld.cpp
        src/gameplay/PlayerState.cpp
        src/gameplay/GameplayWorld.cpp
        src/persistence/SaveGame.cpp
        src/rendering/Mesh.cpp
        src/rendering/SkinnedMesh.cpp
        src/rendering/Texture.cpp
        src/scene/Scene.cpp
    )
    target_include_directories(reflex_engine_combat_tests PRIVATE src)
    target_compile_definitions(reflex_engine_combat_tests PRIVATE
        REFLEX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(reflex_engine_combat_tests PRIVATE glm::glm glad_gl_core_33 nlohmann_json::nlohmann_json reflex_phase6_core)
    reflex_warnings(reflex_engine_combat_tests)
    add_test(NAME combat_system COMMAND reflex_engine_combat_tests)

    add_executable(reflex_engine_phase6_tests tests/Phase6Tests.cpp)
    target_include_directories(reflex_engine_phase6_tests PRIVATE src)
    target_link_libraries(reflex_engine_phase6_tests PRIVATE reflex_phase6_core)
    reflex_warnings(reflex_engine_phase6_tests)
    add_test(NAME phase6_systems COMMAND reflex_engine_phase6_tests)
endif()

add_executable(reflex-asset-tool tools/AssetTool.cpp)
target_include_directories(reflex-asset-tool PRIVATE src)
target_link_libraries(reflex-asset-tool PRIVATE reflex_phase6_core)
reflex_warnings(reflex-asset-tool)

add_custom_target(validate-assets
    COMMAND reflex-asset-tool validate-level
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/campaign/level01.json"
    COMMAND reflex-asset-tool validate-level
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/campaign/level02.json"
    COMMAND reflex-asset-tool validate-weapons
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/combat/combat.json"
    COMMAND reflex-asset-tool validate-enemies
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/combat/combat.json"
    COMMAND reflex-asset-tool validate-audio
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/audio.json"
    COMMAND reflex-asset-tool validate-model
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/levels/phase6_level01.glb"
    COMMAND reflex-asset-tool validate-model
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/levels/phase6_level02.glb"
    COMMAND reflex-asset-tool validate-model
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/grunt.glb"
    COMMAND reflex-asset-tool validate-model
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/pistol_viewmodel.glb"
    COMMAND reflex-asset-tool validate-manifest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/manifest.json"
    DEPENDS reflex-asset-tool
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)

add_custom_command(TARGET reflex_engine POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:reflex_engine>/assets"
    COMMENT "Copying runtime assets"
    VERBATIM
)

# FetchContent builds SDL as a shared library. Keep its DLL beside the executable
# when using a Windows generator so the build tree is directly runnable.
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET reflex_engine POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:reflex_engine>
        VERBATIM
    )
endif()
