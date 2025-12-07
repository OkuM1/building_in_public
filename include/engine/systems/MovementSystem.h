#pragma once
#include "engine/ecs/System.h"
#include "game/components/GameComponents.h"

namespace engine {

class MovementSystem : public System {
public:
    void update(World& world, float dt) override;
};

} // namespace engine
