#include "client/render/Rendering.h"

#include "engine/ecs/Entity.h"
#include "game/components/GameComponents.h"

#include <algorithm>
#include <vector>

namespace client {

namespace {
struct DrawItem {
    float x;
    float y;
    const game::Renderable* renderable;
};
} // namespace

void renderWorld(engine::World& world, Renderer& renderer, float alpha) {
    renderer.Clear();

    std::vector<DrawItem> items;
    items.reserve(64);

    for (engine::EntityId entity = 0; entity < engine::MAX_ENTITIES; ++entity) {
        if (!world.hasComponent<game::Transform>(entity))  continue;
        if (!world.hasComponent<game::Renderable>(entity)) continue;

        auto& transform  = world.getComponent<game::Transform>(entity);
        auto& renderable = world.getComponent<game::Renderable>(entity);

        float x = transform.x;
        float y = transform.y;

        if (world.hasComponent<game::PreviousTransform>(entity)) {
            auto& prev = world.getComponent<game::PreviousTransform>(entity);
            x = prev.x * (1.0f - alpha) + transform.x * alpha;
            y = prev.y * (1.0f - alpha) + transform.y * alpha;
        }

        items.push_back({x, y, &renderable});
    }

    std::sort(items.begin(), items.end(),
        [](const DrawItem& a, const DrawItem& b) {
            return a.renderable->layer < b.renderable->layer;
        });

    for (const auto& item : items) {
        const auto& r = *item.renderable;
        switch (r.shape) {
            case game::Renderable::Shape::Rectangle:
                renderer.RenderRectangle(item.x, item.y, r.width, r.height,
                                         r.r, r.g, r.b);
                break;
            case game::Renderable::Shape::Circle: {
                const float radius = (r.width + r.height) / 4.0f;
                renderer.RenderCircle(item.x, item.y, radius, r.r, r.g, r.b);
                break;
            }
        }
    }
}

} // namespace client
