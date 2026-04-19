#include "engine/systems/InputSystem.h"

#include "engine/ecs/Entity.h"

#include <cmath>

/**
 * @file InputSystem.cpp
 * @brief Implementation of @ref engine::InputSystem.
 *
 * Intentionally contains no platform, windowing, or networking dependency.
 * The input is already sitting on each entity as a `game::PlayerInput`
 * component when this runs; we just translate it.
 *
 * @see docs/design/0002-decouple-inputsystem.md
 */

namespace engine {

namespace {
/// Player movement speed in world units per second. Kept as a file-local
/// constant for now; will become a component when characters gain stats.
constexpr float kPlayerSpeed = 0.5f;
} // namespace

void InputSystem::update(World& world, float /*dt*/) {
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!world.hasComponent<game::PlayerInput>(entity)) continue;
        if (!world.hasComponent<game::Velocity>(entity))    continue;

        const auto& input = world.getComponent<game::PlayerInput>(entity);
        auto&       vel   = world.getComponent<game::Velocity>(entity);

        float vx = 0.0f;
        float vy = 0.0f;
        if (input.moveUp)    vy += kPlayerSpeed;
        if (input.moveDown)  vy -= kPlayerSpeed;
        if (input.moveLeft)  vx -= kPlayerSpeed;
        if (input.moveRight) vx += kPlayerSpeed;

        if (vx != 0.0f && vy != 0.0f) {
            const float inv = kPlayerSpeed / std::sqrt(vx * vx + vy * vy);
            vx *= inv;
            vy *= inv;
        }

        vel.vx = vx;
        vel.vy = vy;
    }
}

} // namespace engine
