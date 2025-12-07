#include "engine/systems/RenderSystem.h"
#include "engine/ecs/Entity.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace engine {

void RenderSystem::update(World& world, float dt) {
    // Clear screen
    renderer.Clear();
    
    // Render grid (optional background)
    renderer.RenderMap(10, 16, 0.1f);
    
    // Collect all entities with Transform + Renderable
    // (Simplified approach - a real ECS would have a view/query system)
    struct RenderableEntity {
        EntityId id;
        game::Transform* transform;
        game::Renderable* renderable;
    };
    
    std::vector<RenderableEntity> renderables;
    
    // Iterate through all possible entity IDs
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (world.hasComponent<game::Transform>(entity) && 
            world.hasComponent<game::Renderable>(entity)) {
            renderables.push_back({
                entity,
                &world.getComponent<game::Transform>(entity),
                &world.getComponent<game::Renderable>(entity)
            });
        }
    }
    
    // Sort by layer (lower layers drawn first)
    std::sort(renderables.begin(), renderables.end(), 
        [](const RenderableEntity& a, const RenderableEntity& b) {
            return a.renderable->layer < b.renderable->layer;
        });
    
    // Render all entities
    for (const auto& re : renderables) {
        const auto& t = *re.transform;
        const auto& r = *re.renderable;
        
        if (r.shape == game::Renderable::Shape::Rectangle) {
            renderer.RenderRectangle(t.x, t.y, r.width, r.height, r.r, r.g, r.b);
        } else if (r.shape == game::Renderable::Shape::Circle) {
            float radius = (r.width + r.height) / 4.0f; // Average for circle radius
            renderer.RenderCircle(t.x, t.y, radius, r.r, r.g, r.b);
        }
    }
}

} // namespace engine
