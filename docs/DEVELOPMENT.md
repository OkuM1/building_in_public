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
- **Active phase:** **Phase 4 ✅ complete** — all 6 sub-commits plus glue. Phase 4 stability milestone passed (150 ms / 5 % loss, 100/100 reliable events delivered). **Phase 5 is next** — replication model.
- **Testbed:** [Sumo Arena](SUMO_ARENA.md) (design locked; no implementation yet)
- **Runnable:**
  - `./build.sh && ./build/client` — windowed client, WASD/arrows + F5/F6/F7 record/stop/playback
  - `./build/server` — headless sim loop at 60 Hz, logs tick counter every second
  - `cd build && ctest --output-on-failure` — 92 cases / 1783 assertions, all green

### Phase status

| Phase | Status | Notes |
|-------|--------|-------|
| 1. Foundation & ECS             | ✅ Done    | ECS, logger, doctest, CI, modern CMake |
| 2. Deterministic simulation     | ✅ Done    | Fixed 60 Hz, interpolation, F5/F6/F7 input record/playback |
| 2.5. Engine / Client split      | ✅ Done    | `simulation` (no GL) / `client_lib` / `server` targets; determinism test green |
| 3. Serialization & snapshots    | ✅ Done    | `BitStream`, quantiser, trait-based snapshot encoder, delta encoder, benchmarks. Delta target ✅, full-snapshot target ❌ (parked). |
| 4. Reliable UDP layer           | ✅ Done    | 4a–4f + glue. `Connection` facade: ack/RTT, 3 channels, congestion. Milestone: 150 ms / 5 % loss ✅. |
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

### Phase 5 — Replication model

Phase 4 closed with a working `Connection` facade. Phase 5 sits entirely
above it — it never touches sockets, headers, or sequence numbers.

**High-level plan (matches ROADMAP):**

1. **Server-authoritative loop.** Clients send `PlayerInput` packets via
   `ChannelId::ReliableUnordered`; server owns the canonical simulation.
   Per-tick loop: drain inbound → apply inputs → advance sim → broadcast
   snapshot delta via `ChannelId::Unreliable`.

2. **Snapshot interpolation.** Remote entities render at ~2-frame delay
   using two buffered snapshots. `lerp(prevSnapshot, currSnapshot, alpha)`.

3. **Client-side prediction.** The local player is stepped immediately on
   input. Predicted state is stored per-tick in a ring buffer.

4. **Server reconciliation.** On receiving the server's ack of an input
   tick, compare predicted state to authoritative state. If they differ
   beyond a threshold, snap to authoritative and replay all pending
   unacked inputs forward.

5. **Lag compensation.** For contact hits (push/shove in Sumo Arena),
   rewind the server to the attacker's view-time and re-evaluate there.

6. **Area of Interest (AoI).** Per-client entity subset. Critical for
   scaling beyond ~32 entities without blowing the bandwidth budget.

**Milestone:** [Sumo Arena](SUMO_ARENA.md) MVP is playable and *feels
good* at 0 / 50 / 100 / 150 ms RTT and 0 / 5 % loss.

### Where `Connection` plugs in (sketch)

```cpp
// Server per-tick:
for (auto& [id, conn] : clients_) {
    for (auto& pkt : socket.recvAll(id))
        conn.receivePacket(pkt.data(), pkt.size(), now);
    while (auto msg = conn.receive(ChannelId::ReliableUnordered))
        applyInput(id, *msg);
    if (conn.shouldSendNow(now)) {
        conn.send(ChannelId::Unreliable, encodeDelta(id));
        socket.send(id, conn.buildPacket(now));
    }
}
```

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

- `Fragmentation` (Phase 4d) is not yet wired into `Connection` / `ReliableChannel`.
  Messages larger than ~1400 bytes should be split at the application level.
  Wiring it in is a Phase 5 follow-up.
- `PROJECT_STRUCTURE.md` describes the *target* structure (some items marked
  *(planned)*); this file is the truth for what's actually on disk.
- `engine/net/` contains all the Phase 4 net primitives. Phase 5 will add
  a replication layer (`engine/replication/`) on top of `Connection`.

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
