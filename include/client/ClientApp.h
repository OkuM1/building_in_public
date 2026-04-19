#pragma once

#include "engine/core/Simulation.h"
#include "engine/core/InputRecorder.h"
#include "client/platform/Renderer.h"
#include "client/input/KeyboardPoller.h"
#include "game/components/GameComponents.h"

#include <GLFW/glfw3.h>

#include <string>

/**
 * @file ClientApp.h
 * @brief Top-level client: owns the window, renderer, input polling,
 *        recorder, and the real-time loop. Drives a headless
 *        `engine::Simulation`.
 *
 * Replaces the transitional `Engine` class from earlier commits. The
 * separation is now:
 *   - `engine::Simulation` \u2014 platform-free fixed-step sim.
 *   - `client::ClientApp`  \u2014 everything you need a keyboard + screen for.
 *
 * @see docs/design/0001-simulation-split.md
 * @see docs/design/0005-introduce-clientapp.md
 */

namespace client {

/// @brief Windowed, input-driven client that ticks a simulation.
class ClientApp {
public:
    ClientApp(int width, int height, const std::string& title);
    ~ClientApp();

    /// @brief Enter the main loop. Returns when the window closes.
    void Run();

    /// @brief Access to the underlying simulation (tests / tooling).
    engine::Simulation&       Simulation()       { return sim_; }
    const engine::Simulation& Simulation() const { return sim_; }

    // Non-copyable: owns a GLFW window.
    ClientApp(const ClientApp&)            = delete;
    ClientApp& operator=(const ClientApp&) = delete;

private:
    void InitWindow();
    void InitSimulation();
    void CreateTestEntities();
    void MainLoop();
    void Cleanup();

    void HandleRecorderHotkeys();
    void PublishPlayerInput();

    // Window / renderer --------------------------------------------------
    int         width_;
    int         height_;
    std::string title_;
    GLFWwindow* window_ = nullptr;
    Renderer    renderer_{};

    // Simulation (platform-free) ----------------------------------------
    engine::Simulation sim_{};

    // Client-side input plumbing ----------------------------------------
    KeyboardPoller        keyboardPoller_{};
    engine::InputRecorder recorder_{};
    engine::EntityId      playerEntity_ = engine::INVALID_ENTITY;

    // Real-time bookkeeping ---------------------------------------------
    float lastFrameTime_ = 0.0f;
    int   frameCount_    = 0;
    float fpsTimer_      = 0.0f;
};

} // namespace client
