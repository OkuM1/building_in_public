# 0003 — Extract `engine::Simulation` from `Engine`

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 2 of 6
- **Related:** [0001](0001-simulation-split.md), [0002](0002-decouple-inputsystem.md), [learning/0003](../learning/0003-headless-first.md)

---

## Context

[0001](0001-simulation-split.md) set the direction: a headless server must
be able to run the simulation without GLFW or OpenGL. [0002](0002-decouple-inputsystem.md)
removed GLFW from `InputSystem`. Now `Engine` still owns the `World`, the
systems vector (including `RenderSystem`), the fixed-timestep accumulator,
and the GLFW window all in one class — so `engine_core` still transitively
depends on GL/GLFW everywhere.

The current main loop mixes three unrelated clocks:

1. **Real time** — `glfwGetTime()`, frame budget, FPS counter.
2. **Simulation time** — the accumulator and `FIXED_TIMESTEP`.
3. **Render time** — the interpolation alpha, run once per real frame.

And the systems vector mixes two unrelated concerns: pure-function sim
systems (movement, input-to-velocity) and a render system that reads the
world to draw. `Engine::Update` uses `dynamic_cast<RenderSystem*>` to skip
the render system — a smell that the vector is doing two jobs.

## Decision

> **Introduce `engine::Simulation` — a pure, platform-free class that
> owns the ECS world, the simulation systems, the fixed-step accumulator,
> and a monotonic `tick` counter. `Engine` keeps windowing, rendering,
> input polling, and the real-time main loop; it delegates the
> simulation clock to `Simulation`.**

Concretely, this commit:

1. Creates [`engine::Simulation`](../../include/engine/core/Simulation.h)
   with:
   - `World world_` (accessor `world()`),
   - `std::vector<std::unique_ptr<System>> systems_` — **simulation
     systems only**,
   - `float accumulator_`, `uint32_t tick_`,
   - `static constexpr float FIXED_DT = 1.0f / 60.0f`,
   - `void addSystem(std::unique_ptr<System>)`,
   - `void step()` — runs exactly one tick (`FIXED_DT`), bumps `tick_`,
   - `void advance(float realDt)` — accumulator loop calling `step()`,
     clamps `realDt` at 0.25 s,
   - `float alpha() const` — `accumulator_ / FIXED_DT` for render
     interpolation,
   - `uint32_t tick() const`.

   `Simulation.h` / `.cpp` **do not include GLFW, OpenGL, or any
   platform header.**

2. Shrinks `Engine`:
   - Its `World`, `systems` vector, `accumulator`, and timestep constants
     move to `Simulation`.
   - `RenderSystem` becomes a plain `std::unique_ptr<RenderSystem>`
     member — no longer hiding inside a polymorphic vector, no more
     `dynamic_cast`. `Engine::Render` calls it directly.
   - `Engine::Update(dt)` is removed; `Engine::MainLoop` calls
     `sim.advance(frameTime)` and `render_->update(sim.world(),
     sim.alpha())`.
   - `Engine::GetWorld()` forwards to `sim_.world()`.

3. `CMakeLists.txt` adds `src/engine/core/Simulation.cpp` to
   `engine_core`. The CMake split into a GL-free `simulation` library
   lands in Commit 5; this commit is the C++ precondition.

## Alternatives considered

- **Leave `RenderSystem` in the systems vector and just move the vector
  into `Simulation`.** Rejected: then `Simulation` has to know about
  rendering, or needs a "skip render" flag, which is the smell we're
  removing.
- **Make `Simulation` a template parameterised on a `Clock`.** YAGNI.
  A single `advance(float)` method is enough; a test can call `step()`
  directly and ignore the real clock.
- **Keep `Engine::Update(float dt)` and have it call `sim_.step()` in a
  loop.** Rejected: then `Engine` still owns the accumulator. The whole
  point is that the simulation clock belongs to the simulation.

## Consequences

**Good**

- The simulation loop (`advance` → `step` → every sim system's `update`)
  is now independent of any platform. A headless `server` main in a
  later commit can construct a `Simulation`, call `advance` or `step` in
  a tight loop, and tick forward — no window required.
- `tick_` gives every other part of the codebase (recorder, networking,
  tests) a monotonic simulation clock that doesn't drift with frame
  rate. This is the clock Phase 4 (UDP) will use for snapshots and acks.
- The `dynamic_cast` in `Engine::Update` is gone. Systems that are in
  `Simulation` are sim systems by construction.
- Determinism tests (Commit 6) become trivial: construct a `Simulation`,
  pre-populate entities and `PlayerInput`, call `step()` N times, assert
  on the world.

**Bad / deferred**

- `Engine` still owns the `KeyboardPoller`, `InputRecorder`, window,
  and renderer. All of those move to `client::ClientApp` in Commits
  3–4. Today they are just grouped on the client-shaped half of a class
  that will split in two.
- The CMake still builds `engine_core` as one library that links GLFW.
  Splitting it into `simulation` + `client` libraries is Commit 5; the
  header-level separation arrives here first so the CMake split is
  mechanical.

## Code pointers

- [`include/engine/core/Simulation.h`](../../include/engine/core/Simulation.h)
- [`src/engine/core/Simulation.cpp`](../../src/engine/core/Simulation.cpp)
- [`include/engine/core/Engine.h`](../../include/engine/core/Engine.h)
- [`src/engine/core/Engine.cpp`](../../src/engine/core/Engine.cpp)

## Acceptance criteria

1. `Simulation.h` and `Simulation.cpp` translation units do not
   transitively include `<GLFW/*>` or `<GL/*>`. (Verified by
   `grep` after build.)
2. Build + unit tests green.
3. Running `./build/main` plays identically to before: player moves at
   the same speed, record/playback still works.
4. `engine::InputSystem`, `engine::MovementSystem` are constructed
   through `Simulation::addSystem`. `RenderSystem` is not.
