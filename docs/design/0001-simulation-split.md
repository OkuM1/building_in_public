# 0001 — Split `Engine` into headless `Simulation` + `ClientApp`

- **Status:** Accepted (not yet implemented)
- **Date:** 2026-04-19
- **Phase:** 2.5
- **Related:** [learning/0001-separating-concerns.md](../learning/0001-separating-concerns.md)

---

## Context

The current `Engine` class (in
[include/engine/core/Engine.h](../../include/engine/core/Engine.h) /
[src/engine/core/Engine.cpp](../../src/engine/core/Engine.cpp)) owns:

- the GLFW window,
- the ECS `World`,
- the list of systems (simulation *and* render),
- the fixed-timestep main loop,
- and FPS/title-bar bookkeeping.

`InputSystem` reads `glfwGetKey` directly and owns an `InputRecorder`.
`RenderSystem` is a `System` sitting in the same vector as simulation
systems, with `Engine::Update` using a `dynamic_cast` to skip it.

The project's stated direction ([ROADMAP.md](../ROADMAP.md)) is an
**engine-first, networked** engine. A headless server must be able to run
the simulation without GLFW or OpenGL. The current coupling makes that
impossible — `engine_core` transitively links GL and GLFW because
`RenderSystem`, `Engine`, and `InputSystem` all depend on them.

## Decision

> **Extract a pure, headless `engine::Simulation` from `Engine`. Move all
> windowing, input polling, rendering, and input recording to a new
> `client::ClientApp`. Split the CMake into a `simulation` library (no GL/GLFW),
> a `client` executable, and a `server` executable that links only `simulation`.**

Concretely:

- `engine::Simulation` owns `World`, the systems list, the fixed-timestep
  accumulator, and the authoritative tick counter (`uint32_t`).
- `Simulation::step()` runs exactly one fixed tick and increments `tick`.
- `Simulation::advance(realDt)` runs the accumulator loop and returns the
  interpolation alpha.
- `InputSystem` stops depending on GLFW. It becomes a pure sim system that
  reads an already-populated `game::PlayerInput` component and writes
  `Velocity`.
- A new `client::KeyboardPoller` (GLFW-side) produces a `game::PlayerInput`
  value each frame, and `ClientApp` writes it onto the player entity before
  calling `Simulation::advance`.
- `RenderSystem` stops being a `System`. It becomes a free function
  `client::renderWorld(World&, Renderer&, float alpha)` called by `ClientApp`
  after `advance`. The `dynamic_cast` hack is deleted.
- `InputRecorder` moves to `ClientApp`. It operates at the `PlayerInput`
  layer, which is the same layer the network will later feed — so Phase 4
  can replace the recorder with a packet source without touching the sim.

## Alternatives considered

- **Option A — keep `Engine`, add `#ifdef HEADLESS` branches.** Rejected.
  Proliferates conditional compilation, doesn't actually decouple anything,
  and fails the sniff test that the simulation library should not *mention*
  rendering.

- **Option B — make `Engine` a template parameterised on a "frontend" type.**
  Rejected. Over-engineered for two frontends; the real split is a runtime
  one (different executables), not a compile-time one.

- **Option C — keep `RenderSystem` as a `System`, add a null renderer for
  the server.** Rejected. A "null" dependency is still a dependency; the
  server would still link GL and its container image would still need
  `libGL.so` available unless we go to heroic static-link gymnastics. Also
  fails the principle that the simulation is not *aware* of rendering.

- **Do nothing.** Rejected. The entire rest of the roadmap (serialization,
  networking, deployment) assumes a simulation that can run without a window.

## Consequences

**Positive**

- The simulation is genuinely testable in isolation. First determinism test
  becomes trivial: step a `Simulation` N times with scripted inputs and
  compare `Transform`s bit-for-bit against a golden vector.
- Phase 4 networking fits naturally: the network layer is another `PlayerInput`
  source, just like `KeyboardPoller` and `InputRecorder`.
- Server and client diverge cleanly. Server container image won't need
  windowing libraries.
- `dynamic_cast` disappears, removing a smell.

**Negative / cost**

- Six files move and two (`Engine`, `RenderSystem`) are deleted. Short-term
  disruption.
- The transition has a few intermediate commits where the architecture is
  half-split; must not regress the playable build along the way.
- `client::` is a new namespace and a new directory tree. Minor mental tax.

**Follow-ups**

- Add a determinism regression test (`tests/test_simulation_determinism.cpp`)
  that fails if any future change breaks bit-identical replay.
- Add a CI job that builds the `server` target in a container **without**
  GL/GLFW installed, so regressions are caught immediately.
- Phase 3 (serialization) can proceed against `Simulation::world()` without
  touching `ClientApp`.

## Code pointers

*After Phase 2.5 lands, these files will carry `@see docs/design/0001-simulation-split.md`:*

- `include/engine/core/Simulation.h` *(new)*
- `src/engine/core/Simulation.cpp` *(new)*
- `include/client/ClientApp.h` *(new)*
- `src/client/ClientApp.cpp` *(new)*
- `src/client/main.cpp` *(new — replaces `src/main.cpp`)*
- `src/server/main.cpp` *(new)*
- `include/client/input/KeyboardPoller.h` *(new)*
- `include/client/render/RenderWorld.h` *(new — replaces `RenderSystem`)*
- `include/engine/systems/InputSystem.h` *(modified — no GLFW)*
- `CMakeLists.txt` *(modified — three targets)*

And files being removed:

- `include/engine/core/Engine.h`, `src/engine/core/Engine.cpp`
- `include/engine/systems/RenderSystem.h`, `src/engine/systems/RenderSystem.cpp`
- `src/main.cpp`

## References

- Robert C. Martin, *Clean Architecture* — the Dependency Rule (inner layers
  may not know about outer layers).
- Scott Meyers, *Effective C++* Item 23 — prefer non-member non-friend
  functions to member functions when they don't need state. Motivates the
  `renderWorld(...)` free function.
- "Fix Your Timestep!" — Glenn Fiedler. The `advance` / `step` split preserves
  the same semantics, just at a layer the server can share.
