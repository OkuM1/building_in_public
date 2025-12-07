#pragma once
#include "engine/ecs/System.h"
#include "game/components/GameComponents.h"
#include "engine/platform/Renderer.h"

namespace engine {

class RenderSystem : public System {
public:
    RenderSystem(Renderer& renderer) : renderer(renderer) {}
    
    void update(World& world, float dt) override;

private:
    Renderer& renderer;
};

} // namespace engine
