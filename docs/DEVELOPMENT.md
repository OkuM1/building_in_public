# Development Guide

> **Living context doc.** Start here after any break. Update it as you go.
>
> Goal: never lose context again. If something here is wrong, fix it in the same
> commit as the change that made it wrong.

---

## 🎯 Project framing

**The engine is the product.** We are building a highly performant, networked
C++ game engine. Games are chosen as **testbeds** that force the hard engine
problems to show up.

- **Primary testbed:** [Sumo Arena](SUMO_ARENA.md) — minimalist 4–16 player
  physics PvP. Picked because it stresses prediction, reconciliation, lag
  comp, and interest management with zero art budget.
- **Later showcase:** the [Dungeon Crawler](GAME_DESIGN.md) parked for after
  the engine hits its performance targets.
- **Success looks like numbers**, not features. See
  [ROADMAP.md → Performance targets](ROADMAP.md#-performance-targets-how-we-know-its-done).

---

## 🚦 Current State (snapshot)

- **Branch:** `master` (clean, tracking `origin/master`)
- **Latest commit:** `321d0a0` — *feat: complete phase 2 — fixed timestep, interpolation, and input recording*
- **Active phase:** Phase 3 ✅ → **Phase 4 in progress** — reliable UDP, split into 6 sub-commits. **4a–4f ✅** (sockets, header+ack+RTT, channels, fragmentation, congestion control, net-sim). Next: **Phase 4 closeout** — glue all pieces into a per-peer facade and run the 150 ms / 5 % stability milestone.
- **Testbed:** [Sumo Arena](SUMO_ARENA.md) (design locked; no implementation yet)
- **Runnable:**
  - `./build.sh && ./build/client` — windowed client, WASD/arrows + F5/F6/F7 record/stop/playback
  - `./build/server` — headless sim loop at 60 Hz, logs tick counter every second
  - `cd build && ctest --output-on-failure` — doctest, includes determinism proof

### Phase status

| Phase | Status | Notes |
|-------|--------|-------|
| 1. Foundation & ECS             | ✅ Done    | ECS, logger, doctest, CI, modern CMake |
| 2. Deterministic simulation     | ✅ Done    | Fixed 60 Hz, interpolation, F5/F6/F7 input record/playback |
| 2.5. Engine / Client split      | ✅ Done    | `simulation` (no GL) / `client_lib` / `server` targets; determinism test green |
| 3. Serialization & snapshots    | ✅ Done    | `BitStream`, quantiser, trait-based snapshot encoder, delta encoder, benchmarks. Delta target ✅, full-snapshot target ❌ (parked). |
| 4. Reliable UDP layer           | ⏳ In progress | **4a–4f ✅** (sockets, header+ack+RTT, channels, fragmentation, congestion, net-sim). Remaining: glue commit + 150 ms / 5 % stability run. |
| 5. Replication model            | ⏳ Planned | Server auth, prediction, reconciliation, lag comp, AoI |
| 5.5. Rollback netcode *(opt.)*  | ⏳ Stretch | Ring-buffer resim on misprediction |
| 6. Deployment & ops             | ⏳ Planned | Docker, Prometheus, VPS soak test |
| 7. Benchmarks, docs, demo       | ⏳ Planned | Perf report, write-ups, README with numbers |

---

## 📂 Where things live

Actual layout (authoritative — `docs/PROJECT_STRUCTURE.md` is aspirational in places):

```
include/
├── engine/
│   ├── core/          Engine.h, Logger.h, InputRecorder.h
│   ├── ecs/           Entity.h, Component.h, World.h, System.h
│   ├── platform/      Renderer.h                (GLFW + OpenGL wrapper)
│   └── systems/       RenderSystem.h, InputSystem.h, MovementSystem.h
└── game/
    ├── components/    GameComponents.h         (Transform, Velocity, Renderable, PlayerInput, tags)
    └── systems/       (empty — reserved for AI/Combat)

src/
├── main.cpp                                    (single entry point for now)
├── engine/{core,ecs,platform,systems}/*.cpp
└── game/systems/                               (empty)

tests/
├── main.cpp                                    (doctest runner)
└── test_logger.cpp
```

**Build targets** (`CMakeLists.txt`):
- `engine_core` — static lib, all engine + systems sources
- `client` and `main` — same binary, both link `engine_core` (alias kept for compat)
- `unit_tests` — doctest suite (built when `BUILD_TESTS=ON`, default)
- `server` — commented out, to be added in Phase 4

**Does not yet exist** despite being referenced elsewhere in docs: `include/legacy/`, `src/client/`, `src/server/`, `scripts/` (build/clean scripts are at the repo root).

---

## 🏗️ Architecture cheatsheet

Three layers, strict downward dependency:

```
game/      (components, game-specific systems)
  ↓
engine/    (ECS, systems, Engine class)
  ↓
platform/  (GLFW window, OpenGL draw calls)  ← currently under engine/platform/
```

### ECS in one minute

Entities are just IDs (`engine::EntityId` = `uint32_t`). Components are plain data
structs in `game::` namespace. Systems implement `engine::System` and mutate
the `engine::World` each tick.

```cpp
engine::World& w = engine.GetWorld();
w.registerComponent<game::Transform>();          // once at init
auto e = w.createEntity();
w.addComponent(e, game::Transform{0, 0, 0});
auto& t = w.getComponent<game::Transform>(e);
bool has = w.hasComponent<game::Transform>(e);
```

Component storage is a packed array per type (`ComponentArray<T>`), indexed by
entity via two `unordered_map`s. `MAX_ENTITIES` / `MAX_COMPONENTS` are compile-time
constants in `include/engine/ecs/Component.h`.

### Game loop (Phase 2)

Fixed timestep in `Engine::MainLoop`:

```
accumulator += realDt
while (accumulator >= FIXED_TIMESTEP)      // FIXED_TIMESTEP = 1/60
    snapshot PreviousTransform              // for interpolation
    Update(FIXED_TIMESTEP)                  // all systems
    accumulator -= FIXED_TIMESTEP
alpha = accumulator / FIXED_TIMESTEP
Render(alpha)                               // lerp(PreviousTransform, Transform, alpha)
```

### Input recording (determinism proof)

`engine::InputRecorder` serialises `game::PlayerInput` per tick.

| Key | Action |
|-----|--------|
| F5  | Start recording |
| F6  | Stop recording |
| F7  | Playback (overrides live input) |
| ESC | Quit |
| WASD / Arrows | Move player |

---

## 🛠️ Common commands

```bash
# Build (Debug, writes to ./build/)
./build.sh

# Clean rebuild
./clean.sh && ./build.sh

# Run game
./build/main

# Run tests
cd build && ctest --output-on-failure

# Run tests with verbose doctest output
./build/unit_tests --success
```

Compile DB is emitted at `build/compile_commands.json` — symlink/copy to repo
root if your editor/clangd needs it there.

---

## 🧭 Resuming after a break (5-minute checklist)

1. `git status && git log --oneline -10` — what did past-you do?
2. Read the top of this file (**Current State**) and **Next up**.
3. Skim [DEVLOG.md](DEVLOG.md) for the latest week entry.
4. `./build.sh && ./build/main` — confirm nothing is broken.
5. `cd build && ctest --output-on-failure` — tests green?
6. Pick the top item from **Next up** and create a `feature/…` branch.

---

## 🎯 Next up (ordered)

### Phase 2.5 — Engine / Client split (this week)

**Commit 1 ✅ — Decouple `InputSystem` from GLFW.** Done.
See [design/0002](design/0002-decouple-inputsystem.md) and
[learning/0002](learning/0002-data-as-a-seam.md). `client::KeyboardPoller`
now produces `PlayerInput`; `InputSystem` reads the component. Build + tests
green.

**Commit 2 ✅ — Extract `engine::Simulation`.** Done. See
[design/0003](design/0003-extract-simulation.md) and
[learning/0003](learning/0003-headless-first.md). `Simulation` owns world,
sim systems, accumulator, tick counter. `Engine` delegates. `Simulation.cpp`
translation unit pulls in zero GLFW/GL headers (verified with `g++ -H`).

**Commit 3 ✅ — Rendering as a free function.** Done. See
[design/0004](design/0004-render-as-free-function.md) and
[learning/0004](learning/0004-polymorphism-for-sameness.md). Deleted
`RenderSystem`; replaced with `engine::renderWorld(World&, Renderer&, float)`.
`Engine` no longer owns a `unique_ptr<RenderSystem>`. `System` now
unambiguously means “sim system”.

**Next, Commit 4 — Introduce `client::ClientApp`:** move window ownership,
`KeyboardPoller`, `InputRecorder`, and the render call out of `Engine` into
a new `client::ClientApp`. `Engine` either retires or shrinks to a thin
alias.

1. **Introduce `engine::Simulation`** in `include/engine/core/Simulation.h`
   + `src/engine/core/Simulation.cpp`:
   - Owns the `World`, the list of systems, the accumulator, and a new
     `uint32_t tick` counter.
   - Exposes `step(fixedDt)` (runs exactly one tick) and `advance(realDt)`
     (runs the accumulator loop). **No GLFW, no OpenGL includes.**
2. **Introduce `client::ClientApp`** (or similar) that owns the window,
   renderer, input polling, and calls `Simulation::advance`.
3. **Shrink `Engine`**: move the windowing + render parts into `ClientApp`
   and retire `Engine` (or keep it as a thin alias during transition).
4. **CMake refactor:**
   - `simulation` target — static lib, only engine sources with no GLFW.
   - `client` target — depends on `simulation` + renderer + GLFW.
   - Commented-out `server` target becomes real, gated by `-DENGINE_HEADLESS`,
     linking only `simulation`.
   - Drop the duplicate `main` target in favour of `client`.
5. **Tick as clock**: replace internal `float dt` accounting with `uint32_t tick`
   + `constexpr float FIXED_DT`. Systems still receive `dt` where convenient.
6. **Tests:** a `Simulation` stepped N times with recorded inputs produces
   identical transforms (determinism test).

**Exit criteria:** `./build/server` builds and runs (even if it just ticks
forever and logs tick count) on a box with no GL libraries.

### Then Phase 3 — Serialization & snapshots

See [ROADMAP.md → Phase 3](ROADMAP.md#phase-3--serialization--snapshots) for
the full checklist. High level:

1. `engine::net::BitStream` + quantised types.
2. Per-component `serialize` traits.
3. Full-world snapshot encode/decode.
4. Delta encoding with baseline-ack.
5. `bench/` scaffold and first committed numbers in `bench/results.md`.

### What happened to "collision / AI / combat"?

Deferred. Those live inside the [Sumo Arena](SUMO_ARENA.md) testbed
(`CollisionSystem`, `PhysicsSystem`, `DashSystem`, `ShoveSystem`) and land
**after** the Engine/Client split and serialization, because there's no
point building gameplay on the old coupled `Engine`.

---

## 📏 Conventions (quick ref, see [CONTRIBUTING.md](../CONTRIBUTING.md) for detail)

- C++17, `#pragma once`, 4-space indent, 100-col soft limit.
- `PascalCase` types & files, `camelCase` functions/vars, `SCREAMING_SNAKE` constants,
  lowercase namespaces (`engine`, `game`).
- One class per file; `.h` in `include/<ns-path>/`, `.cpp` mirror in `src/`.
- Include order: own header → engine headers → game headers → std.
- Components = pure data, no methods. Logic goes in systems.
- **New source files must be added to the `engine_core` target in [CMakeLists.txt](../CMakeLists.txt).**

---

## ⚠️ Known drift / gotchas

- `main` and `client` CMake targets build the same binary. Both get dropped /
  merged during Phase 2.5; `client` becomes the real client, `server` appears.
- `Engine` currently owns the GLFW window *and* the ECS world. Phase 2.5
  splits this into a headless `Simulation` + `ClientApp`. **Do not** add new
  features to `Engine` as-is — add them to `Simulation` after the split or
  you'll be moving them twice.
- `engine/platform/` currently only has `Renderer.h`. Sockets land in a
  sibling `engine/net/` during Phase 4.
- Input is read from GLFW directly inside `InputSystem`. After the split,
  `InputSystem` should receive a `game::PlayerInput` *value* per tick so the
  headless server can synthesise or replay inputs without GLFW.
- `PROJECT_STRUCTURE.md` describes the *target* structure (some items marked
  *(planned)*); this file is the truth for what's actually on disk.

---

## 🗒️ Update protocol

When you land a change that affects any of the above:
1. Update **Current State** (commit hash, phase).
2. Tick/move the item in **Next up**.
3. Fix **Where things live** or **Known drift** if the layout changed.
4. Add a one-paragraph entry to [DEVLOG.md](DEVLOG.md).
5. For non-trivial changes, add / update a [design note](design/) and a
   [learning note](learning/). See [CODE_DOCS.md](CODE_DOCS.md).

Doc changes go in the same commit as the code change that caused them.
