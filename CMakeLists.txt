cmake_minimum_required(VERSION 3.24)

project(ReflexEngine
    VERSION 0.1.0
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
)

target_include_directories(reflex_engine PRIVATE src)

target_link_libraries(reflex_engine PRIVATE
    SDL3::SDL3
    glad_gl_core_33
    glm::glm
)

if(MSVC)
    target_compile_options(reflex_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(reflex_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
