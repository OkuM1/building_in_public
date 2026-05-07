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
- **Active phase:** **Phase 5a ✅ complete** — `InputMessage`, `SnapshotBuffer`, `PredictionBuffer` landed; full integration test green. **Phase 5b is next** — `ReplicationServer` + `ReplicationClient` manager classes.
- **Testbed:** [Sumo Arena](SUMO_ARENA.md) (design locked; no implementation yet)
- **Runnable:**
  - `./build.sh && ./build/client` — windowed client, WASD/arrows + F5/F6/F7 record/stop/playback
  - `./build/server` — headless sim loop at 60 Hz, logs tick counter every second
  - `cd build && ctest --output-on-failure` — 110 cases / 1864 assertions, all green

### Phase status

| Phase | Status | Notes |
|-------|--------|-------|
| 1. Foundation & ECS             | ✅ Done    | ECS, logger, doctest, CI, modern CMake |
| 2. Deterministic simulation     | ✅ Done    | Fixed 60 Hz, interpolation, F5/F6/F7 input record/playback |
| 2.5. Engine / Client split      | ✅ Done    | `simulation` (no GL) / `client_lib` / `server` targets; determinism test green |
| 3. Serialization & snapshots    | ✅ Done    | `BitStream`, quantiser, trait-based snapshot encoder, delta encoder, benchmarks. Delta target ✅, full-snapshot target ❌ (parked). |
| 4. Reliable UDP layer           | ✅ Done    | 4a–4f + glue. `Connection` facade: ack/RTT, 3 channels, congestion. Milestone: 150 ms / 5 % loss ✅. |
| 5. Replication model            | 🔄 5a done | `InputMessage`, `SnapshotBuffer`, `PredictionBuffer`. Integration test ✅. 5b next: `ReplicationServer`/`ReplicationClient` managers. |
| 5.5. Rollback netcode *(opt.)*  | ⏳ Stretch | Ring-buffer resim on misprediction |
| 6. Deployment & ops             | ⏳ Planned | Docker, Prometheus, VPS soak test |
| 7. Benchmarks, docs, demo       | ⏳ Planned | Perf report, write-ups, README with numbers |

---

## 📂 Where things live

Actual layout (authoritative — `docs/PROJECT_STRUCTURE.md` is aspirational in places):

```
include/
├── engine/
│   ├── core/          Engine.h, Logger.h, InputRecorder.h, Simulation.h
│   ├── ecs/           Entity.h, Component.h, World.h, System.h
│   ├── net/           BitStream, Quantize, Serialize, Socket, PacketHeader,
│   │                  ReliableEndpoint, Channel, Fragmentation,
│   │                  CongestionController, SimulatedLink, Connection
│   ├── replication/   InputMessage.h, SnapshotBuffer.h, PredictionBuffer.h   ← Phase 5a
│   └── systems/       InputSystem.h, MovementSystem.h
└── game/
    └── components/    GameComponents.h, ComponentSerializers.h

src/
├── main.cpp                                    (client entry point)
├── engine/{core,ecs,net,replication,systems}/*.cpp
├── client/                                     ClientApp, render, input
└── server/                                     main_server.cpp

tests/
├── main.cpp                                    (doctest runner)
├── test_{logger,simulation_determinism,bitstream,quantize,...}.cpp
└── test_replication.cpp                        (Phase 5a)
```

**Build targets** (`CMakeLists.txt`):
- `simulation` — headless static lib (no GL): engine + net + replication + systems
- `client_lib` — windowed pieces; links `simulation` + GLFW + OpenGL
- `client` — windowed executable
- `server` — headless executable; links `simulation` only
- `unit_tests` — doctest suite (built when `BUILD_TESTS=ON`, default)

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

### Phase 5b — ReplicationServer + ReplicationClient managers

Phase 5a landed the three replication primitives:
- `InputMessage` — wire format for client→server inputs
- `SnapshotBuffer` — client-side interpolation ring buffer
- `PredictionBuffer` — client-side prediction + reconciliation buffer

Phase 5b composes them into manager classes:

1. **`ReplicationServer`** — per-client session on the server side:
   - Holds the client's `EntityId`
   - `receiveInput(InputMessage)` — queues inputs
   - `applyInput(world, tick)` — applies queued inputs at the right sim tick
   - `maybeSendSnapshot(conn, world, tick)` — encodes delta and sends
   - `onSnapshotAcked(tick)` — advances the delta baseline

2. **`ReplicationClient`** — on the client side:
   - `onTick(input, localTransform)` — sends input, records prediction
   - `onSnapshotReceived(payload)` — decodes, pushes to SnapshotBuffer,
     reconciles against PredictionBuffer
   - `getRenderTransform(entityId, renderTick)` — hands back interpolated
     or predicted transform to the renderer

3. **Area of Interest (AoI)** — per-client entity subset filter.
   - Server only sends entities within a radius of each client's player.
   - Critical for scaling: 32-entity game with 16 clients × 30 Hz × delta
     snapshots fits < 32 KB/s per client only if AoI is tight.

4. **Lag compensation** — for contact/push hits in Sumo Arena:
   - Server keeps a ring buffer of world snapshots (last N ticks).
   - On a push event, rewind to the attacker's view-time and re-evaluate
     the collision from there.

**Milestone (Phase 5):** [Sumo Arena](SUMO_ARENA.md) MVP is playable and
*feels good* at 0 / 50 / 100 / 150 ms RTT and 0 / 5 % loss.

### Where everything plugs in (sketch)

```cpp
// Server per-tick (Phase 5b):
for (auto& [id, session] : clients_) {
    for (auto& pkt : socket.recvAll(id))
        session.conn.receivePacket(pkt.data(), pkt.size(), now);
    session.repl.drainInputs(session.conn, world);  // apply queued inputs
}
sim.step();
for (auto& [id, session] : clients_) {
    session.repl.maybeSendSnapshot(session.conn, world, sim.tick());
    if (session.conn.shouldSendNow(now))
        socket.send(id, session.conn.buildPacket(now));
}

// Client per-tick (Phase 5b):
repl.onTick(localInput, localTransform);          // predict + send
for (auto& pkt : socket.recvAll(serverId))
    conn.receivePacket(pkt.data(), pkt.size(), now);
repl.drainSnapshots(conn);                         // decode + reconcile
// Render:
renderTransform = repl.getRenderTransform(entityId, renderTick);
```

---

## 📏 Conventions (quick ref, see [CONTRIBUTING.md](../CONTRIBUTING.md) for detail)

- C++17, `#pragma once`, 4-space indent, 100-col soft limit.
- `PascalCase` types & files, `camelCase` functions/vars, `SCREAMING_SNAKE` constants,
  lowercase namespaces (`engine`, `game`).
- One class per file; `.h` in `include/<ns-path>/`, `.cpp` mirror in `src/`.
- Include order: own header → engine headers → game headers → std.
- Components = pure data, no methods. Logic goes in systems.
- **New source files must be added to the `simulation` target in [CMakeLists.txt](../CMakeLists.txt).**

---

## ⚠️ Known drift / gotchas

- `Fragmentation` (Phase 4d) is not yet wired into `Connection` / `ReliableChannel`.
  Messages larger than ~1400 bytes should be split at the application level.
  Wiring it in is a Phase 5 follow-up.
- `PROJECT_STRUCTURE.md` describes the *target* structure (some items marked
  *(planned)*); this file is the truth for what's actually on disk.
- `engine/replication/` (Phase 5a) holds the three replication primitives.
  The manager classes (`ReplicationServer`, `ReplicationClient`) are Phase 5b.

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
