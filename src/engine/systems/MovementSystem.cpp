#include "engine/systems/MovementSystem.h"
#include "engine/ecs/Entity.h"

namespace engine {

void MovementSystem::update(World& world, float dt) {
    // Apply velocity to all entities with Transform + Velocity
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!world.hasComponent<game::Transform>(entity) ||
            !world.hasComponent<game::Velocity>(entity)) {
            continue;
        }
        
        auto& transform = world.getComponent<game::Transform>(entity);
        auto& velocity = world.getComponent<game::Velocity>(entity);
        
        // Update position
        transform.x += velocity.vx * dt;
        transform.y += velocity.vy * dt;
        
        // Simple boundary clamping (to keep entities on screen)
        const float worldSize = 0.9f;
        if (transform.x < -worldSize) transform.x = -worldSize;
        if (transform.x > worldSize) transform.x = worldSize;
        if (transform.y < -worldSize) transform.y = -worldSize;
        if (transform.y > worldSize) transform.y = worldSize;
    }
}

} // namespace engine
