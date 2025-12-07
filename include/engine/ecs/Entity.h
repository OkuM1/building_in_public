#pragma once
#include <cstdint>
#include <limits>

namespace engine {

// Entity is just an ID
using EntityId = uint32_t;

// Invalid entity constant
constexpr EntityId INVALID_ENTITY = std::numeric_limits<EntityId>::max();

// Maximum number of entities supported
constexpr EntityId MAX_ENTITIES = 10000;

} // namespace engine
