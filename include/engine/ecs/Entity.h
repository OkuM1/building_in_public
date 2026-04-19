#pragma once
#include <cstdint>
#include <limits>

/**
 * @file Entity.h
 * @brief Entity identifier type and ECS-wide invariants.
 *
 * In this ECS an entity is nothing more than an integer handle. All data
 * that a game cares about (position, velocity, renderable, etc.) lives in
 * component arrays keyed by this id. Keeping the handle narrow and trivially
 * copyable is deliberate: it lets us pass entities by value everywhere and
 * eventually serialise them cheaply across the network.
 *
 * @see docs/design/0001-simulation-split.md
 * @see docs/CODE_DOCS.md  Header-comment style we follow.
 */

namespace engine {

/// @brief Opaque handle identifying an entity.
///
/// Entities carry no state themselves. 32 bits is plenty for a single-server
/// simulation and serialises as a variable-length int without special-casing.
using EntityId = uint32_t;

/// @brief Sentinel value meaning "no entity".
///
/// Returned by APIs that may fail to resolve an entity (for example, AI
/// target lookup when no target is in range). Treat any result equal to
/// this as a miss, not as a valid entity.
constexpr EntityId INVALID_ENTITY = std::numeric_limits<EntityId>::max();

/// @brief Compile-time upper bound on concurrently live entities.
///
/// Packed component arrays are sized from this constant, so raising it
/// trades RAM for headroom. Keep it a round power-of-ten for now;
/// revisit when we have a profiler saying otherwise.
constexpr EntityId MAX_ENTITIES = 10000;

} // namespace engine
