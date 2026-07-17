cmake_minimum_required(VERSION 3.24)

project(ReflexEngine
    VERSION 0.4.0
    DESCRIPTION "A 1999-2004-inspired first-person shooter engine"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

add_executable(reflex_engine
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/input/InputState.cpp
    src/collision/CollisionQueries.cpp
    src/collision/CollisionWorld.cpp
    src/collision/DynamicCollisionWorld.cpp
    src/gameplay/PlayerController.cpp
    src/gameplay/PlayerState.cpp
    src/gameplay/GameplayWorld.cpp
    src/persistence/SaveGame.cpp
    src/debug/DebugDraw.cpp
    src/rendering/Mesh.cpp
    src/rendering/HudRenderer.cpp
    src/rendering/Renderer.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp
    src/scene/Camera.cpp
    src/scene/GltfLoader.cpp
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
)

if(MSVC)
    target_compile_options(reflex_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(reflex_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(reflex_engine_camera_tests
        tests/CameraTests.cpp
        src/input/InputState.cpp
        src/scene/Camera.cpp
    )
    target_include_directories(reflex_engine_camera_tests PRIVATE src)
    target_link_libraries(reflex_engine_camera_tests PRIVATE SDL3::SDL3 glm::glm)
    if(MSVC)
        target_compile_options(reflex_engine_camera_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(reflex_engine_camera_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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
    if(MSVC)
        target_compile_options(reflex_engine_collision_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(reflex_engine_collision_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME collision_math COMMAND reflex_engine_collision_tests)

    add_executable(reflex_engine_gameplay_tests
        tests/GameplayTests.cpp
        src/collision/CollisionQueries.cpp
        src/collision/CollisionWorld.cpp
        src/collision/DynamicCollisionWorld.cpp
        src/gameplay/PlayerState.cpp
        src/gameplay/GameplayWorld.cpp
        src/persistence/SaveGame.cpp
        src/rendering/Mesh.cpp
        src/rendering/Texture.cpp
    )
    target_include_directories(reflex_engine_gameplay_tests PRIVATE src)
    target_link_libraries(reflex_engine_gameplay_tests PRIVATE glm::glm glad_gl_core_33 nlohmann_json::nlohmann_json)
    if(MSVC)
        target_compile_options(reflex_engine_gameplay_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(reflex_engine_gameplay_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME gameplay_persistence COMMAND reflex_engine_gameplay_tests)
endif()

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
