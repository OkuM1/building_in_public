#pragma once
#include "World.h"

namespace engine {

class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, float dt) = 0;
};

} // namespace engine
