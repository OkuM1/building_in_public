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
# simulation: platform-free static library (no GLFW / no OpenGL).
# Links against this from the server executable to prove the headless
# dependency graph works.
# ============================================================================
add_library(simulation STATIC
    src/engine/core/Simulation.cpp
    src/engine/core/Logger.cpp
    src/engine/core/InputRecorder.cpp

    src/engine/ecs/World.cpp

    src/engine/net/BitStream.cpp
    src/engine/net/Quantize.cpp
    src/engine/net/Socket.cpp
    src/engine/net/PacketHeader.cpp
    src/engine/net/ReliableEndpoint.cpp
    src/engine/net/Channel.cpp
    src/engine/net/Fragmentation.cpp
    src/engine/net/CongestionController.cpp
    src/engine/net/SimulatedLink.cpp
    src/engine/net/Connection.cpp

    src/engine/systems/InputSystem.cpp
    src/engine/systems/MovementSystem.cpp
)

target_include_directories(simulation
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Enforce the headless invariant: the simulation library must not link
# any windowing/GL library. See docs/design/0006-cmake-split.md.
get_target_property(_sim_link_libs simulation LINK_LIBRARIES)
if(_sim_link_libs MATCHES "glfw|OpenGL")
    message(FATAL_ERROR
        "simulation target must not link glfw/OpenGL (got: ${_sim_link_libs})")
endif()

# ============================================================================
# client_lib: all the windowed/GL pieces that sit on top of `simulation`.
# ============================================================================
add_library(client_lib STATIC
    src/client/ClientApp.cpp
    src/client/input/KeyboardPoller.cpp
    src/client/platform/Renderer.cpp
    src/client/render/Rendering.cpp
)

target_include_directories(client_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(client_lib
    PUBLIC
        simulation
        glfw
        OpenGL::GL
)

# ============================================================================
# Client executable (windowed game)
# ============================================================================
add_executable(client src/main.cpp)
target_link_libraries(client PRIVATE client_lib)

# ============================================================================
# Server executable (headless - links `simulation` only)
# ============================================================================
add_executable(server src/server/main_server.cpp)
target_link_libraries(server PRIVATE simulation)

# ============================================================================
# Tests
# ============================================================================
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    enable_testing()
    
    add_executable(unit_tests
        tests/main.cpp
        tests/test_logger.cpp
        tests/test_simulation_determinism.cpp
        tests/test_bitstream.cpp
        tests/test_quantize.cpp
        tests/test_snapshot.cpp
        tests/test_delta.cpp
        tests/test_socket.cpp
        tests/test_reliable_endpoint.cpp
        tests/test_channel.cpp
        tests/test_fragmentation.cpp
        tests/test_congestion.cpp
        tests/test_simulated_link.cpp
        tests/test_connection.cpp
    )
    
    target_link_libraries(unit_tests PRIVATE simulation doctest::doctest)
    
    # Ensure we can find doctest.h
    target_include_directories(unit_tests PRIVATE ${doctest_SOURCE_DIR}/doctest)

    add_test(NAME unit_tests COMMAND unit_tests)
endif()

# ============================================================================
# Benchmarks (google-benchmark, opt-in).
#
# Built Release-only by default to keep numbers meaningful. Use
#
#     cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCH=ON
#     cmake --build build-bench --target bench_snapshot
#     ./build-bench/bench_snapshot
#
# Results are committed to `bench/results.md` per roadmap convention.
# ============================================================================
option(BUILD_BENCH "Build benchmarks (google-benchmark)" OFF)
if(BUILD_BENCH)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(bench_snapshot bench/bench_snapshot.cpp)
    target_link_libraries(bench_snapshot PRIVATE simulation benchmark::benchmark)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS client server
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