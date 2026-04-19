#pragma once

#include "engine/ecs/World.h"
#include "engine/ecs/System.h"

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @file Simulation.h
 * @brief Platform-free owner of the ECS world and the fixed-step sim clock.
 *
 * `Simulation` is deliberately ignorant of windows, rendering, networking,
 * and wall-clock time. It owns the `World`, a list of simulation `System`s,
 * an accumulator, and a monotonic `tick` counter. You feed it real-time
 * deltas with @ref advance, or drive it directly with @ref step for tests,
 * replays, and the headless server.
 *
 * The translation units for this class must not include any GLFW or
 * OpenGL headers. That is the invariant that makes a headless server
 * possible.
 *
 * @see docs/design/0001-simulation-split.md
 * @see docs/design/0003-extract-simulation.md
 */

namespace engine {

/// @brief Headless, deterministic fixed-step simulation.
class Simulation {
public:
    /// @brief Fixed simulation timestep (60 Hz). Do not change per-frame.
    static constexpr float FIXED_DT = 1.0f / 60.0f;

    Simulation() = default;

    /// @brief Add a simulation system. Call before the first `advance` /
    ///        `step`. Systems are run in insertion order each tick.
    void addSystem(std::unique_ptr<System> system);

    /// @brief Run exactly one fixed-step tick. Bumps the tick counter.
    ///        Pure function of current world state + whatever producers
    ///        wrote onto entities since the last step.
    void step();

    /// @brief Drain real-time into fixed steps. Clamps `realDt` at 0.25 s
    ///        to prevent spiral-of-death after a big pause.
    void advance(float realDt);

    /// @brief Interpolation factor in [0, 1) for the render layer.
    float alpha() const { return accumulator_ / FIXED_DT; }

    /// @brief Monotonic simulation tick count. Never decreases.
    std::uint32_t tick() const { return tick_; }

    /// @brief Mutable access to the ECS world.
    World&       world()       { return world_; }
    const World& world() const { return world_; }

private:
    World world_{};
    std::vector<std::unique_ptr<System>> systems_{};
    float         accumulator_ = 0.0f;
    std::uint32_t tick_        = 0;
};

} // namespace engine
