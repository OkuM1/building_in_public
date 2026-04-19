#pragma once

#include "engine/ecs/System.h"
#include "game/components/GameComponents.h"

/**
 * @file InputSystem.h
 * @brief Translates per-entity `game::PlayerInput` into `game::Velocity`.
 *
 * `InputSystem` is a **pure simulation system**: it does not read the
 * keyboard, touch GLFW, or know where the `PlayerInput` value came from.
 * The value is written onto entities by a producer outside the simulation
 * (today: `client::KeyboardPoller`; later: network, recorder, AI bot), and
 * this system just consumes it.
 *
 * Treating input as *data on entities* rather than *calls from a source* is
 * the seam that lets the same simulation run on a headless server without
 * any platform dependencies.
 *
 * @see docs/design/0001-simulation-split.md
 * @see docs/design/0002-decouple-inputsystem.md
 * @see game::PlayerInput
 */

namespace engine {

/// @brief Consumes `game::PlayerInput` and writes `game::Velocity`.
///
/// For every entity that has both components, sets velocity from the input
/// flags at a constant speed, with diagonal movement normalised so players
/// don't get a free speed boost from pressing two keys.
class InputSystem : public System {
public:
    /// @brief Construct the system. No dependencies: input arrives as data.
    InputSystem() = default;

    /// @brief Apply inputs to velocities for all matching entities.
    ///
    /// @param world  ECS world; reads `PlayerInput`, writes `Velocity`.
    /// @param dt     Fixed simulation timestep; unused today because the
    ///               output is an instantaneous velocity, but kept for
    ///               interface symmetry with other systems.
    void update(World& world, float dt) override;
};

} // namespace engine
