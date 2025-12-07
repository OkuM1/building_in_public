#include "engine/systems/RenderSystem.h"
#include "engine/ecs/Entity.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace engine {

void RenderSystem::update(World& world, float alpha) {
    // Clear screen
    renderer.Clear();
    
    // Collect all entities with Transform + Renderable
    struct RenderableEntity {
        EntityId id;
        float x, y; // Interpolated position
        game::Renderable* renderable;
    };
    
    std::vector<RenderableEntity> renderables;
    
    // Iterate through all possible entity IDs
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (world.hasComponent<game::Transform>(entity) && 
            world.hasComponent<game::Renderable>(entity)) {
            
            auto& transform = world.getComponent<game::Transform>(entity);
            auto& renderable = world.getComponent<game::Renderable>(entity);
            
            float x = transform.x;
            float y = transform.y;
            
            // Interpolate if we have previous state
            if (world.hasComponent<game::PreviousTransform>(entity)) {
                auto& prev = world.getComponent<game::PreviousTransform>(entity);
                x = prev.x * (1.0f - alpha) + transform.x * alpha;
                y = prev.y * (1.0f - alpha) + transform.y * alpha;
            }

            renderables.push_back({entity, x, y, &renderable});
        }
    }
    
    // Sort by layer (lower layers drawn first)
    std::sort(renderables.begin(), renderables.end(), 
        [](const RenderableEntity& a, const RenderableEntity& b) {
            return a.renderable->layer < b.renderable->layer;
        });
    
    // Render all entities
    for (const auto& re : renderables) {
        const auto& r = *re.renderable;
        
        if (r.shape == game::Renderable::Shape::Rectangle) {
            renderer.RenderRectangle(re.x, re.y, r.width, r.height, r.r, r.g, r.b);
        } else if (r.shape == game::Renderable::Shape::Circle) {
            float radius = (r.width + r.height) / 4.0f; // Average for circle radius
            renderer.RenderCircle(re.x, re.y, radius, r.r, r.g, r.b);
        }
    }
}

} // namespace engine
