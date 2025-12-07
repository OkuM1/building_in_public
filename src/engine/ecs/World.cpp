#include "engine/ecs/World.h"
#include <cassert>

namespace engine {

World::World() {
    // Initialize available entities queue
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        availableEntities.push(entity);
    }
}

EntityId World::createEntity() {
    assert(livingEntityCount < MAX_ENTITIES && "Too many entities in existence.");

    EntityId id = availableEntities.front();
    availableEntities.pop();
    livingEntityCount++;

    return id;
}

void World::destroyEntity(EntityId entity) {
    assert(entity < MAX_ENTITIES && "Entity out of range.");

    // Invalidate signature
    signatures[entity].reset();

    // Remove entity's data from all component arrays
    for (auto const& pair : componentArrays) {
        auto const& component = pair.second;
        component->entityDestroyed(entity);
    }

    // Make ID available again
    availableEntities.push(entity);
    livingEntityCount--;
}

} // namespace engine
