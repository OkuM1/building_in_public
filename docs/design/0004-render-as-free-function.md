# 0004 — Rendering as a free function, not a `System`

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 3 of 6
- **Related:** [0003](0003-extract-simulation.md), [learning/0004](../learning/0004-polymorphism-for-sameness.md)

---

## Context

After [0003](0003-extract-simulation.md), simulation systems live in
`engine::Simulation::systems_`, and rendering lives on `Engine` as a
`std::unique_ptr<engine::RenderSystem>`. But `RenderSystem` still derives
from `engine::System` — an abstract base whose whole purpose is "I am a
thing that runs inside `Simulation::step()` each fixed tick". Rendering
does not run per tick. It runs once per real frame with an interpolation
alpha. The only reason `RenderSystem` inherits from `System` is
historical: it used to share the systems vector with `InputSystem` and
`MovementSystem`, back when `Engine::Update` filtered them apart with a
`dynamic_cast`. That vector is now split in two, and the inheritance is
load-bearing for nothing.

Consequences of keeping the inheritance:

- The `update(World&, float dt)` signature lies about what the "dt"
  parameter means. In sim systems it is `FIXED_DT`. In the render
  system it is the interpolation alpha `[0, 1)`. Same type, same name,
  totally different semantics.
- Anyone reading `RenderSystem : public System` is invited to put it
  back into the systems vector.
- A free function is trivially easier to call from a future
  `client::ClientApp` (Commit 4) than a class that the app has to own,
  construct, and forward a `Renderer&` into.

## Decision

> **Replace `engine::RenderSystem` with a free function
> `engine::renderWorld(World& world, Renderer& renderer, float alpha)`
> in `include/engine/systems/Rendering.h` /
> `src/engine/systems/Rendering.cpp`. Delete `RenderSystem.h/.cpp`.**

Concretely, in this commit:

1. New header [`include/engine/systems/Rendering.h`](../../include/engine/systems/Rendering.h)
   declares one function:

   ```cpp
   namespace engine {
   void renderWorld(World& world, Renderer& renderer, float alpha);
   }
   ```

2. New translation unit
   [`src/engine/systems/Rendering.cpp`](../../src/engine/systems/Rendering.cpp)
   holds the body that was previously in `RenderSystem::update` —
   clearing, gathering `{Transform, Renderable}` entities, sorting by
   `layer`, interpolating with `PreviousTransform`, dispatching to
   `renderer.RenderRectangle` / `RenderCircle`.

3. `include/engine/systems/RenderSystem.h` and
   `src/engine/systems/RenderSystem.cpp` are deleted.

4. `Engine` drops the `std::unique_ptr<engine::RenderSystem> render_`
   member. Its render call becomes
   `engine::renderWorld(sim.world(), renderer, sim.alpha());`.

5. `CMakeLists.txt` swaps `RenderSystem.cpp` for `Rendering.cpp` in the
   `engine_core` sources.

## Alternatives considered

- **Keep the class, drop the inheritance.** Would work, but `Renderer&
  renderer_` and a constructor for a one-method class is a lot of
  scaffolding for no benefit. The free function *is* the minimum
  interface.
- **Use `alpha` as a named parameter object
  (`engine::RenderParams{alpha, camera, ...}`).** YAGNI. There's one
  parameter today, and a camera/viewport will be a separate commit
  when it actually exists.
- **Put rendering under `client/render/`.** Tempting — rendering is
  client-side. But `game::Renderable` and the interpolation logic
  against `PreviousTransform` are engine concerns, not client-specific.
  The *driver* (who owns the window, when to call it) is client; the
  *mechanism* (how to walk the world and emit draws) is engine. Keep
  the mechanism in `engine/systems/` and let `ClientApp` in Commit 4
  be the one that calls it.

## Consequences

**Good**

- `engine::System` now unambiguously means "runs inside
  `Simulation::step()` each fixed tick". Any class deriving from it is
  by construction a sim system.
- The render path is a single free function; testing or swapping
  backends (software rasteriser, debug overlay, server screenshot for
  regression tests) only has to provide a different function with the
  same signature.
- Commit 4 (`ClientApp`) just has to `#include
  "engine/systems/Rendering.h"` and call the function. No ownership of
  a render object, no lifetime juggling.

**Bad / deferred**

- The free function still takes `World&` (non-const) because
  `World::hasComponent` and `getComponent` aren't `const`-correct. That
  is a separate refactor and not worth dragging into this commit.
- Rendering is still temporally coupled to `Renderer::Clear` happening
  first and `glfwSwapBuffers` happening after (in `Engine`). Pulling
  that coupling out is Commit 4's problem.

## Code pointers

- [`include/engine/systems/Rendering.h`](../../include/engine/systems/Rendering.h)
- [`src/engine/systems/Rendering.cpp`](../../src/engine/systems/Rendering.cpp)
- [`include/engine/core/Engine.h`](../../include/engine/core/Engine.h)
- [`src/engine/core/Engine.cpp`](../../src/engine/core/Engine.cpp)
- [`CMakeLists.txt`](../../CMakeLists.txt)

## Acceptance criteria

1. `engine::RenderSystem` no longer exists anywhere in the source tree.
2. The only function declared in `Rendering.h` is `renderWorld`.
3. `Engine` has no `RenderSystem` member and no `std::unique_ptr` for
   rendering. It calls `engine::renderWorld` directly in its frame loop.
4. Build + unit tests green. Game plays identically (player still
   moves, enemies still draw, record/playback still works).
