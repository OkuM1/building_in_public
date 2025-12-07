cmake_minimum_required(VERSION 3.16)
project(MultiplayerEngine 
    VERSION 0.1.0
    DESCRIPTION "A multiplayer game engine built for learning"
    LANGUAGES CXX
)

# ============================================================================
# Build Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# ============================================================================
# Core Library (shared between client and server)
# ============================================================================
add_library(engine_core STATIC
    # Engine Core
    src/engine/core/Engine.cpp
    
    # Platform Layer
    src/engine/platform/Renderer.cpp
    
    # ECS
    src/engine/ecs/World.cpp
    
    # Systems
    src/engine/systems/RenderSystem.cpp
    src/engine/systems/InputSystem.cpp
    src/engine/systems/MovementSystem.cpp
    
    # Future:
    # src/engine/core/Logger.cpp
    # src/game/systems/AISystem.cpp
    # src/game/systems/CombatSystem.cpp
)

target_include_directories(engine_core 
    PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(engine_core 
    PUBLIC
        glfw
        OpenGL::GL
)

# ============================================================================
# Client Executable
# ============================================================================
add_executable(client src/main.cpp)
target_link_libraries(client PRIVATE engine_core)

# Alias for backward compatibility
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE engine_core)

# ============================================================================
# Server Executable (headless - to be implemented)
# ============================================================================
# add_executable(server src/server/main_server.cpp)
# target_link_libraries(server PRIVATE engine_core)
# target_compile_definitions(server PRIVATE HEADLESS_SERVER)

# ============================================================================
# Tests (to be implemented)
# ============================================================================
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    # find_package(GTest REQUIRED)
    # enable_testing()
    # add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS client main
    RUNTIME DESTINATION bin
)

# ============================================================================
# CPack Configuration (for packaging)
# ============================================================================
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "Multiplayer Engine Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "")