# Development Log

> Weekly progress updates on building the multiplayer engine.
> For the current in-flight state, see [DEVELOPMENT.md](DEVELOPMENT.md).

---

## Week 7: Phase 5a — replication model primitives

**Phase 5a shipped.** Server-authoritative loop, snapshot interpolation, and
client-side prediction + reconciliation all implemented and tested.

### Completed

- **`engine::replication::InputMessage`.** Wire format for client→server
  player inputs. `varint(tick)` + 6 packed bools. `serializeInput` /
  `deserializeInput` round-trip cleanly. Every input carries the client's
  simulation tick so the server can schedule it on its own timeline and echo
  back a confirmed-up-to tick for reconciliation.

- **`engine::replication::SnapshotBuffer`.** Client-side ring buffer of decoded
  server snapshots. Stores `(EntityId, Transform)` per entity per tick (not raw
  bytes). `interpolate(entityId, renderTick)` linearly blends between the two
  bracketing snapshots, returning `std::nullopt` when the entity is absent in
  either. Buffer capacity: 16 snapshots ≈ 0.8 s of 20-Hz history. Out-of-order
  arrivals are silently discarded.

- **`engine::replication::PredictionBuffer`.** Client-side ring buffer of
  `(tick, PlayerInput, predictedTransform)`. `ackUpTo(T)` discards confirmed
  entries. `getPending(T)` returns all unconfirmed entries after T for replay.
  Capacity: 128 ticks ≈ 2.1 s at 60 Hz — enough headroom for any realistic RTT.

- **End-to-end integration test.** Server runs `Simulation` + `InputSystem` +
  `MovementSystem`. Client sends `moveRight = true` for 180 ticks via
  `Connection::ReliableUnordered`. Server applies each input, steps the sim,
  broadcasts unreliable snapshots. Client receives, decodes into
  `SnapshotBuffer`, and interpolates. All assertions green:
  - Server entity x > 0 after 180 ticks (movement applied).
  - SnapshotBuffer populated; `canInterpolate` true at midpoint.
  - Interpolated x > 0 (entity moved right on the server's canonical sim).
  - PredictionBuffer capped at 128; `ackUpTo` + `getPending` return correct counts.

- **Reconciliation unit test.** 60 ticks of `moveRight=true` prediction at
  tick 30 receives a server snapshot with x = 0 (blocked). `ackUpTo(30)` +
  `getPending(30)` + replay produces the correct corrected state (29 ×
  kDxPerTick from x = 0 — not 60 × kDxPerTick as the client predicted).

### Numbers
| Entity | Result |
|---|---|
| InputMessage size (tick < 128, 0 buttons) | 2 bytes |
| InputMessage size (tick < 128, all buttons) | 2 bytes |
| SnapshotBuffer memory (16 slots × 16 entities × 16 B) | 4 KB |
| PredictionBuffer memory (128 entries × ~32 B) | ~4 KB |

### Tests
110 test cases / 1864 assertions all green. New: 18 `test_replication.cpp`
cases covering all three primitives plus the reconciliation flow and the full
integration loop.

### Docs
- Design: [0020 replication model](design/0020-replication-model.md).
- Learning: [0020](learning/0020-prediction-and-trust.md) — Prediction and
  Trust: Why the Client Never Believes Itself.

### Learnings
- Prediction is trivial to implement; **reconciliation** is where the
  interesting engineering lives. The buffer primitives make it mechanical.
- Storing *decoded* state (not raw bytes) in the snapshot buffer trades
  4 KB of extra memory for zero-cost interpolation queries — the right
  trade for a rendering hot path.
- The render delay insight: you don't interpolate forward to "now". You
  interpolate backward to "a moment you have two data points for". This is
  the entire reason snapshot buffering works.

### Next
- Phase 5b: `ReplicationServer` + `ReplicationClient` manager classes that
  compose these three primitives into a full per-session state machine.
  Then: Area of Interest filtering, lag compensation.

---



**Phase 4 closed.**

### Completed
- **Loss detection in `ReliableEndpoint`.** Added a two-vector overload of
  `processInboundHeader` that, after resolving acks, scans 64 slots beyond
  the 32-slot ack window and declares any un-acked sent sequences as lost.
  The original single-vector overload is unchanged; all existing tests pass
  without modification.
- **`engine::net::Connection` (per-peer facade).** Wires together
  `ReliableEndpoint`, `UnreliableChannel`, `ReliableChannel` (×2), and
  `CongestionController`. Wire format: `[12-byte header] [ch0] [ch1] [ch2]`,
  each channel section self-framing with a varint message count.
  `buildPacket` always produces a valid packet (acks flow even with no
  payload). `shouldSendNow` gates sends via the congestion controller.
- **Phase 4 stability milestone ✅.** Two `Connection` objects over two
  `SimulatedLink`s at 75 ms one-way / 5 % loss (= 150 ms RTT / 5 % loss)
  deliver all 100 reliable-unordered events from A to B, maintain a
  measured RTT estimate of ~150 ms, and remain stable for 12 simulated
  seconds plus a 3-second drain.
- **Pre-existing build fix.** Added missing `#include <array>` to
  `include/engine/ecs/World.h` (masked by stdlib transitives in CI; exposed
  on GCC 13 in the sandbox).

### Numbers
| Target | Result | Status |
|---|---|---|
| 150 ms RTT / 5 % loss stable | 100/100 events delivered | ✅ |
| RTT estimate in range | ~150 ms (within 50–500 ms check) | ✅ |

### Tests
92 cases / 1783 assertions all green. New: 9 `test_connection.cpp` cases
(unit tests + milestone test).

### Docs
- Design: [0019 connection glue](design/0019-connection-glue.md).
- Learning: [0019](learning/0019-the-glue-is-not-the-boring-part.md) —
  why the glue is not the boring part.

### Learnings
- A method that nobody calls (`onPacketLost`) is a promise deferred. It
  works only when the surrounding machinery exists to invoke it. The glue
  commit closes promises left open by sub-commits.
- Loss detection needs no timers if you already have the peer's ack
  horizon and your own sent buffer. The information was always there.
- Integration tests rarely fail when units are solid. The milestone test
  passed on the first run.

### Next
- Phase 5: replication model. Server-authoritative simulation, clients
  send inputs only. Snapshot interpolation, client-side prediction, server
  reconciliation, lag compensation. `Connection` is the send/receive
  surface; Phase 5 sits entirely above it.

---

## Week 5: Phase 3 — serialization & snapshots complete

**All 5 commits shipped. Phase 3 closed.**

### Completed
- **Commit 1 — `engine::net::BitStream`.** MSB-first bit packing,
  byte-aligned typed ops, LEB128 varints. The primitive everything
  else in Phase 3 consumes.
- **Commit 2 — quantised types.** 16 bits/axis for position over
  `[-16, +16]` (sub-pixel error), 8 bits for angle. Centralised wire
  constants in `Quantize.h`. A full `Transform` now costs 40 bits
  instead of 96.
- **Commit 3 — trait-based snapshot encoder.** `Serializer<T>` primary
  template + `ReplicationList<Components...>` + template
  `encodeSnapshot`/`decodeSnapshot`. Engine side knows zero component
  types; game side writes one specialisation per replicated component.
  Wire format: `varint tick | varint count | {varint id, u8 mask,
  packed components}*`.
- **Commit 4 — delta encoding.** `Serializer<T>::equal` (quantised
  comparison for `Transform` so sub-quantum drift doesn't trigger
  spurious deltas); `encodeDelta`/`applyDelta` with explicit baseline
  tick and separate changed/removed lists. Caught and fixed a
  mid-commit bug: the "changed" bit rule must be asymmetric
  (components present in *current* only) or the encoder crashes.
- **Commit 5 — benchmarks.** `bench_snapshot` with google-benchmark,
  Release-only, opt-in behind `-DBUILD_BENCH=ON`. Committed results
  in [`bench/results.md`](../bench/results.md).

### Numbers vs roadmap
| Target                                     | Result  | Status |
|--------------------------------------------|---------|:------:|
| Typical delta snapshot < 200 B             | 82 B    | ✅ |
| Full snapshot @ 1k entities < 4 KB         | 15.9 KB | ❌ (parked — deferred to Phase 3.5) |

Steady-state traffic is deltas; full-snapshot miss affects first-join
latency only. Cost decomposition and follow-up items recorded in
`bench/results.md`.

### Tests
30 cases / 135 assertions across `BitStream`, quantiser, snapshot,
and delta. All green.

### Docs
- Design: [0008 BitStream](design/0008-bitstream.md) / [0009 quantisation](design/0009-quantization.md) / [0010 snapshot format](design/0010-snapshot-format.md) / [0011 delta encoding](design/0011-delta-encoding.md) / [0012 benchmarks](design/0012-benchmarks.md).
- Learning: [0008](learning/0008-wire-format-discipline.md) / [0009](learning/0009-quantization-throwing-away-bits.md) / [0010](learning/0010-traits-over-branches.md) / [0011](learning/0011-the-bug-fix-was-the-point.md) / [0012](learning/0012-measurement-beats-opinion.md).

### Learnings
- When a phase has a numeric contract, the last commit is the one
  that measures it. Publish the miss.
- Equality for serializable types should be defined at the *wire
  level*, not raw-float level. Sub-quantum drift otherwise defeats
  the whole delta pipeline.
- Mechanism-in-engine, policy-in-game (serializers) keeps the engine
  template-only and symbol-free.

### Next
- Phase 4: reliable UDP layer. Six sub-commits: sockets, packet
  header + ack, channels, fragmentation, congestion, network sim.
  The snapshot bytes produced in Phase 3 are the payload going into
  those UDP packets.

---

## Week 4: Phase 2.5 — Commits 4–6, engine/client split complete

**Commits 4, 5, 6 of 6 in Phase 2.5. Phase 2.5 closed.**

### Completed
- **Commit 4 — `client::ClientApp`, retire `Engine`.** Deleted
  `engine::Engine` and its header. Created `client::ClientApp` owning the
  window, `client::Renderer`, `engine::Simulation`, `KeyboardPoller`,
  `InputRecorder`, and player entity. `main.cpp` is now five lines.
- **Commit 5 — CMake split + headless `server` target.** Moved
  `Renderer` and `Rendering` out of `engine/` into `client/` (namespace
  `client::`). Split library into `simulation` (no GL) and `client_lib`
  (+GLFW/OpenGL). New `server` executable links `simulation` only; ticks
  the sim at 60 Hz via `steady_clock`/`sleep_until`. Added a CMake
  `FATAL_ERROR` assertion so configure aborts if `simulation` ever picks
  up a GL link dep. Verified: `nm libsimulation.a | grep gl` empty,
  `ldd ./build/server` lists no GL libs.
- **Commit 6 — determinism test.** `tests/test_simulation_determinism.cpp`
  (3 cases, 13 assertions): two `Simulation`s fed the same canned input
  schedule reach bitwise-equal state; a closed-form movement check pins
  the absolute behaviour; a seeded-random 600-tick stream reproduces.
  Tests link `simulation` only.

### Build graph now
```
simulation    (static, no GL)   ── used by ── server, unit_tests, client_lib
client_lib    (+ GLFW + OpenGL) ── used by ── client
```

### Docs
- [design/0005-introduce-clientapp.md](design/0005-introduce-clientapp.md) / [learning/0005-retiring-the-god-class.md](learning/0005-retiring-the-god-class.md)
- [design/0006-cmake-split.md](design/0006-cmake-split.md) / [learning/0006-the-build-graph-is-the-architecture.md](learning/0006-the-build-graph-is-the-architecture.md)
- [design/0007-determinism-test.md](design/0007-determinism-test.md) / [learning/0007-test-the-invariant.md](learning/0007-test-the-invariant.md)

### Learnings
- When a class is labelled “transitional”, give it a deletion commit or
  it becomes permanent. Delete > alias for a pre-0.1 repo.
- Architecture you care about should be a build failure when violated.
  Reviewer-vigilance is not architecture; it is optimism.
- Test the contract (same inputs → same state), not the innards (which
  system touched which field first). Testable only costs nothing once
  the layering makes it cheap — that was the whole point of 2.5.

### Next
- Phase 3: serialization & snapshots. `BitStream`, quantised `Transform`,
  delta encoding, micro-benchmarks. The `simulation` target is now the
  surface the snapshot code reads from, and `server` is the process
  that will emit snapshots.

---

## Week 4: Phase 2.5 — Commit 3, rendering as a free function

**Commit 3 of 6 in Phase 2.5.**

### Completed
- Deleted `engine::RenderSystem`. Replaced with the free function
  `engine::renderWorld(World&, Renderer&, float alpha)` in
  `include/engine/systems/Rendering.h` / `.cpp`.
- `Engine` dropped its `std::unique_ptr<RenderSystem>` member; the frame
  loop now calls `engine::renderWorld(sim.world(), renderer, sim.alpha())`
  directly.
- `engine::System` now unambiguously means “runs inside
  `Simulation::step()` each fixed tick”. No more mixed-meaning `dt`.
- CMake swapped `RenderSystem.cpp` for `Rendering.cpp`.
- Build + unit tests green.

### Docs
- [design/0004-render-as-free-function.md](design/0004-render-as-free-function.md).
- [learning/0004-polymorphism-for-sameness.md](learning/0004-polymorphism-for-sameness.md).

### Learnings
- A class with one public method and a captured reference is a function
  with an expensive call site. Write the function.
- Inheritance is a claim of substitutability. `RenderSystem` and
  `InputSystem` were never substitutable — their `dt` parameters meant
  different things. Sharing a base was a historical accident.
- “What it does” (engine concern: walk ECS + draw) and “who calls it”
  (client concern: own the window) belong in different directories.

### Next
- Commit 4: introduce `client::ClientApp`. Move the window, keyboard
  poller, recorder, and render call out of `Engine`.

---

## Week 4: Phase 2.5 — Commit 2, Simulation extracted

**Commit 2 of 6 in Phase 2.5.**

### Completed
- Added `engine::Simulation` (`include/engine/core/Simulation.h` + `.cpp`):
  owns the `World`, the list of sim systems, a fixed-step accumulator, and
  a monotonic `uint32_t tick` counter. `FIXED_DT = 1/60`. Methods
  `step()`, `advance(float)`, `alpha()`, `tick()`, `world()`.
- Shrunk `Engine` to delegate to `Simulation` for all simulation concerns.
  `RenderSystem` is now a direct `unique_ptr` member, out of the
  polymorphic systems vector — the `dynamic_cast` filter in the update
  loop is gone.
- Verified the headless invariant with `g++ -H`: `Simulation.cpp`'s
  transitive include tree contains no GLFW or GL headers. The simulation
  TU is ready to compile in a GL-less environment (CMake split comes in
  Commit 5).
- Build + unit tests green on Linux.

### Docs
- [design/0003-extract-simulation.md](design/0003-extract-simulation.md).
- [learning/0003-headless-first.md](learning/0003-headless-first.md).

### Learnings
- The sign that a decoupling is real is a mechanical check, not an
  opinion: compile the lower layer, list its transitive includes, assert
  nothing banned appears.
- Two kinds of thing in one container + a filter = a missing type
  distinction. Split the container instead.
- A simulation that owns its clock can be handed to a test, a replay, or
  a server. One whose clock lives in someone else's main loop is stuck
  there.

### Next
- Commit 3: rendering as a free function / prepare to move render out of
  `Engine` entirely into a future `ClientApp`.

---

## Week 4: Phase 2.5 begins — InputSystem decoupled from GLFW

**Commit 1 of 6 in Phase 2.5.**

### Completed
- Introduced `client::KeyboardPoller` (`include/client/input/KeyboardPoller.h`
  / `.cpp`) — the **only** place in the codebase that mentions
  `GLFW_KEY_*`. Produces a `game::PlayerInput` value from a `GLFWwindow*`.
- Rewrote `engine::InputSystem` as a pure simulation system: default
  constructor, no GLFW dependency, no ownership of `InputRecorder`. Reads
  the already-populated `PlayerInput` component and writes `Velocity`.
- Moved `InputRecorder` + F5/F6/F7 hotkey logic into `Engine` (a
  temporary home — it will migrate to `ClientApp` in Commit 3/4).
- Added `src/client/input/KeyboardPoller.cpp` to the `engine_core` CMake
  sources under a "client (temporary home)" comment, previewing the
  target split to come.
- Build + unit tests green on Linux.

### Docs
- [design/0002-decouple-inputsystem.md](design/0002-decouple-inputsystem.md)
  — the ADR governing this commit.
- [learning/0002-data-as-a-seam.md](learning/0002-data-as-a-seam.md) —
  why a `PlayerInput` component beats an `IInputSource` interface.

### Learnings
- The ECS already models "entity that has player intent". Reusing that
  component as the seam is cheaper than introducing a new polymorphism.
  The component *is* the interface — no `IInputSource` needed (YAGNI win).
- A decoupling is only real when the lower layer can be deleted and the
  upper still compiles. Goal for later commits: the sim library compiles
  with GLFW absent from `find_package`.

### Next
- Commit 2: extract `engine::Simulation` from `Engine`. Owns world, systems,
  accumulator, tick counter. No GLFW, no OpenGL.

---

## Week 3: Engine-first reframe + Sumo Arena testbed

**No code commit — planning session.**

### Decisions
- Reframed the project from "a dungeon crawler that happens to be networked"
  to **engine-first**: the engine is the deliverable, games are testbeds.
- Selected **[Sumo Arena](SUMO_ARENA.md)** as the primary testbed. It forces
  every hard netcode problem (prediction, reconciliation, lag compensation,
  AoI) with minimum art and design surface. The dungeon crawler is parked
  as a later showcase on the same engine.
- Committed to concrete performance targets (see
  [ROADMAP.md → targets](ROADMAP.md#-performance-targets-how-we-know-its-done))
  — 60 Hz server with 16 clients, <32 KB/s per client, <4 KB full snapshot
  @ 1k entities, playable at 150 ms RTT + 5% loss, 100 players per $5/mo VPS.
- **Inserted Phase 2.5** — extract `engine::Simulation` from `Engine` so a
  headless server is actually possible. Must land before Phase 3.
- Split Phase 4 (UDP) into 4a–4f (sockets, headers/acks, channels,
  fragmentation, congestion, in-process network simulator).
- Added Phase 5.5 (rollback netcode) as an optional stretch — our
  determinism work from Phase 2 makes it tractable.

### Doc updates
- Rewrote [ROADMAP.md](ROADMAP.md) with engine-first framing + perf targets.
- Added [SUMO_ARENA.md](SUMO_ARENA.md) — testbed design doc (components,
  systems, network notes, MVP scope).
- Updated [DEVELOPMENT.md](DEVELOPMENT.md) phase table and "next up".
- Updated [README.md](../README.md) to lead with the engine, not the game.

### Next
- Land Phase 2.5: extract `Simulation` and `ClientApp`, wire a real `server`
  CMake target, retire the duplicate `main` target.

---

## Week 2: Deterministic Game Loop (Phase 2 complete)

**Commit:** `321d0a0` — *feat: complete phase 2 — fixed timestep, interpolation, and input recording*

### Completed
- Fixed 60 Hz simulation loop following Glenn Fiedler's "Fix Your Timestep".
- Decoupled render rate from physics tick; render interpolates between
  `PreviousTransform` and `Transform` using the loop's accumulator alpha.
- `InputRecorder` serialises per-tick `PlayerInput` to disk; F5 record / F6
  stop / F7 playback. Determinism verified — replay reproduces identical motion.
- FPS counter written to the GLFW window title each second.

### Learnings
- Keeping components as pure data made recording trivial: the recorder only
  has to snapshot `PlayerInput`, not any derived state.
- Interpolation needs the *previous* transform captured **before** the fixed
  update, otherwise single-frame jumps smear across the render.

### Next
- Phase 2 leftovers: AABB collision, pursuit AI, health/combat.
- Then Phase 3: binary serialization format for network-ready snapshots.

---

## Week 1: Project Setup

**Date:** December 7, 2025

### Goals
- [x] Create development roadmap
- [x] Set up documentation structure
- [x] Upgrade CMake configuration
- [x] Add CI/CD pipeline

### Completed
- Created comprehensive [ROADMAP.md](ROADMAP.md) with 7 development phases
- Wrote [ARCHITECTURE.md](ARCHITECTURE.md) documenting the target system design
- Added [CONTRIBUTING.md](../CONTRIBUTING.md) with code style guidelines
- Upgraded CMakeLists.txt to modern CMake with proper targets
- Set up GitHub Actions for CI

### Challenges
- (To be filled as development progresses)

### Learnings
- (To be filled as development progresses)

### Next Week
- Begin Phase 1: ECS Architecture implementation
- Set up Google Test for unit testing
- Create basic logging system

---

## Template for Future Weeks

```markdown
## Week N: [Title]

**Date:** [Date]

### Goals
- [ ] Goal 1
- [ ] Goal 2
- [ ] Goal 3

### Completed
- Item 1
- Item 2

### Challenges
- Challenge and how I solved it

### Learnings
- Key insight learned

### Code Highlights
\`\`\`cpp
// Interesting code snippet from this week
\`\`\`

### Next Week
- Plan for next week
```

---

*This devlog serves as both a personal record and a demonstration of consistent progress for portfolio purposes.*
