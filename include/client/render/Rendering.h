#pragma once

#include "engine/ecs/World.h"
#include "client/platform/Renderer.h"

/**
 * @file Rendering.h
 * @brief Walks the ECS world and emits draw calls. Client-side because
 *        it links the (OpenGL) @ref client::Renderer.
 *
 * @see docs/design/0004-render-as-free-function.md
 * @see docs/design/0006-cmake-split.md
 */

namespace client {

/// @brief Draw every entity with `game::Transform` + `game::Renderable`.
///
/// Entities are sorted by `Renderable::layer` (ascending). If an entity
/// also has `game::PreviousTransform`, its position is linearly
/// interpolated between the previous and current transform using `alpha`.
///
/// @param world     ECS world to read.
/// @param renderer  Platform renderer.
/// @param alpha     Interpolation factor in `[0, 1)` from
///                  `engine::Simulation::alpha()`.
void renderWorld(engine::World& world, Renderer& renderer, float alpha);

} // namespace client
