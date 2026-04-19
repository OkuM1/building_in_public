# Engine-First Roadmap

> A learning-focused roadmap for building a **highly performant, networked C++
> game engine**. The engine is the product; games are testbeds that force us
> to build it right.

---

## 🎯 Project goals

1. **Learn** — deep understanding of engine architecture, networking, systems programming.
2. **Build** — a functional multiplayer engine that can be deployed to the cloud.
3. **Showcase** — a portfolio piece with *measured* performance numbers, not vibes.

**Testbed game:** [Sumo Arena](SUMO_ARENA.md) — minimalist physics PvP chosen
because it stresses every hard netcode problem (prediction, reconciliation,
lag comp, interest management) with zero art budget. The [Dungeon Crawler](GAME_DESIGN.md)
is parked as a later showcase on the same engine.

---

## 📏 Performance targets (how we know it's "done")

Numbers are committed to `bench/results.md` and tracked over time.

| Metric | Target |
|---|---|
| Server tick rate, sustained            | 60 Hz with 16 clients |
| Simulated entities (server)            | 1k active, headroom for 10k |
| Downstream bandwidth per client        | < 32 KB/s @ 20 Hz snapshots |
| Upstream bandwidth per client          | < 4 KB/s @ 60 Hz inputs |
| End-to-end input → render latency (LAN)| < 1 frame |
| Playable over simulated RTT            | 150 ms |
| Smooth over simulated packet loss      | 5 % (playable at 15 %) |
| Full snapshot @ 1k entities            | < 4 KB |
| Typical delta snapshot                 | < 200 B |
| Concurrent players per $5/mo VPS       | ≥ 100 |

---

## 🗺️ Phases

### Phase 1 — Foundation & ECS ✅ *done*
- [x] ECS (entities, packed component arrays, world, system interface)
- [x] Logger (thread-safe, color-coded)
- [x] Unit tests (doctest) + CI (GitHub Actions)
- [x] Modern CMake, clang-format, MIT license

---

### Phase 2 — Deterministic simulation ✅ *done*
- [x] Fixed 60 Hz update loop (Glenn Fiedler's "Fix Your Timestep")
- [x] Decoupled render rate with interpolation via `PreviousTransform`
- [x] Input recording & playback (`F5` / `F6` / `F7`) — determinism proven
- [x] FPS counter

---

### Phase 2.5 — Engine/Client split (prep for headless server)
**Why now:** `Engine` currently owns the GLFW window *and* the world. A
headless server needs `Simulation` with zero GLFW dependency. Do this before
Phase 3 or we pay for it twice.

- [ ] Extract `engine::Simulation` — owns `World`, systems, fixed-tick loop, `tick` counter (no GLFW).
- [ ] Extract `client::ClientApp` — owns window, renderer, input polling, calls into `Simulation`.
- [ ] New CMake targets: `simulation` (static lib, headless-safe), `client` (links `simulation` + renderer), `server` (links `simulation` only, gated by `-DENGINE_HEADLESS`).
- [ ] Drop the duplicate `main` target in favour of `client`.
- [ ] Sim time uses `uint32_t tick`, not `float dt` — tick is the authoritative clock for networking.

**Milestone:** `./build/server` builds and runs a headless simulation on a machine without OpenGL.

---

### Phase 3 — Serialization & snapshots
**Why:** everything after this depends on compact, fast snapshot encoding.

- [ ] `engine::net::BitStream` — bit-level read/write, variable-length ints.
- [ ] Quantised types: position (16-bit fixed point), angle (8-bit), bool packing.
- [ ] Per-component `serialize(BitStream&)` (traits template; no RTTI).
- [ ] Full world snapshot encode/decode.
- [ ] **Delta encoding:** per-component dirty flags, baseline-ack + delta snapshots.
- [ ] `bench/` scaffold (google-benchmark wired into CMake); track encode/decode throughput and snapshot size.

**Milestone:** 1k-entity full snapshot < 4 KB; typical delta < 200 B; committed benchmarks.

---

### Phase 4 — Reliable UDP layer (split; this is the hardest phase)

#### 4a. Sockets
- [ ] Cross-platform non-blocking UDP socket wrapper (`engine::net::Socket`) for Linux + Windows.
- [ ] Linux: use `recvmmsg` / `sendmmsg` for batched syscalls.

#### 4b. Packet header + ack system
- [ ] 16-byte header: sequence, ack, ack bitfield (32 previous packets), channel id.
- [ ] Round-trip time estimation (smoothed RTT).

#### 4c. Channels
- [ ] Unreliable (snapshots).
- [ ] Reliable-unordered (events like eliminations).
- [ ] Reliable-ordered (lobby state, round start/end).

#### 4d. Fragmentation
- [ ] Fragment messages > MTU; reassemble on receive; drop on missing fragments after timeout.

#### 4e. Congestion control
- [ ] RTT-based send-rate scaling (good / bad network modes, per Gaffer).

#### 4f. Network simulator
- [ ] In-process `SimulatedLink` injecting latency / jitter / loss / reordering so the whole stack is testable without a second machine.

**Benchmarks:** throughput, loss behaviour, jitter buffer effectiveness.

**Milestone:** two processes exchange reliable + unreliable messages over loopback with injected 150 ms / 5 % loss and remain stable.

---

### Phase 5 — Replication model
- [ ] Server-authoritative simulation — clients send inputs, never game state.
- [ ] Snapshot interpolation for remote entities (~100 ms render delay).
- [ ] Client-side prediction for the local player.
- [ ] Server reconciliation: on snapshot ACK, snap to authoritative state and replay pending inputs.
- [ ] Lag compensation for contact hits (rewind server state to attacker's view-time).
- [ ] **Area of Interest / relevance filtering** — per-client entity subset. Critical for scaling.

**Milestone:** [Sumo Arena](SUMO_ARENA.md) MVP is playable and feels good at 0 / 50 / 100 / 150 ms RTT and 0 / 5 % loss.

---

### Phase 5.5 — Rollback netcode *(optional but high-signal)*
- [ ] Ring buffer of `N` recent sim snapshots.
- [ ] On misprediction / late remote input: restore snapshot, resimulate forward.
- [ ] Compare against snapshot-interpolation path under a flag; document trade-offs.

**Why:** determinism work from Phase 2 makes this tractable. Rollback netcode is a genuinely hireable niche.

---

### Phase 6 — Deployment & ops
- [ ] Static-linked Linux server binary.
- [ ] Distroless Docker image.
- [ ] Prometheus metrics endpoint on the server: tick time, snapshot size p50/p95/p99, clients connected, packet loss, CPU.
- [ ] Structured JSON logging in server mode.
- [ ] Soak test: 100 simulated clients against a server on a $5/mo VPS-class instance.
- [ ] One-command deploy script.

**Milestone:** friends can connect from different locations and play; server metrics are observable from a browser.

---

### Phase 7 — Benchmarks, docs, demo
- [ ] Formal performance report with flame graphs and bandwidth histograms in `docs/`.
- [ ] Final architecture diagrams (reality, not aspiration).
- [ ] Short demo video (30–60 s).
- [ ] Technical write-ups, at least two of:
  - "A 4 KB world snapshot: bit-packing an ECS."
  - "Reliable UDP from scratch in C++."
  - "Rollback netcode for non-fighters."
  - "Client-side prediction when your state is an ECS."
- [ ] README leads with the numbers, not the plan.

**Milestone:** anyone looking at the repo in 60 seconds understands what it does and how fast it is.

---

## 🧭 Technology choices

| Component | Choice | Rationale |
|---|---|---|
| Language          | C++17                    | Modern enough, industry standard |
| Build             | CMake + presets          | Cross-platform, standard |
| Graphics (client) | OpenGL + GLFW            | Already in; client-only after Phase 2.5 |
| Networking        | Custom UDP               | This *is* the learning project |
| Serialization     | Custom bit-level         | Required for < 4 KB snapshots |
| Tests             | doctest                  | Already integrated, low ceremony |
| Benchmarks        | google-benchmark         | Industry standard microbench tool |
| CI                | GitHub Actions           | Free, integrated |
| Containers        | Docker (distroless)      | Small, standard |
| Cloud             | Small VPS (DO / Hetzner) | Cheap, real-world constraints |
| Metrics           | Prometheus text format   | Scrapable, human-readable |

---

## 📅 Log template

Entries live in [DEVLOG.md](DEVLOG.md).

```markdown
## Week N — [Title]

**Commit:** `<sha>`

### Goals
- [ ] ...

### Completed
- ...

### Challenges / learnings
- ...

### Numbers
- <new benchmark data if any>

### Next
- ...
```

---

## 🚀 Immediate next move

Phase 2.5 — the `Engine` → `Simulation` + `ClientApp` split. Small, focused,
unblocks every subsequent phase. See [DEVELOPMENT.md](DEVELOPMENT.md) for
the current ordered task list.
# Multiplayer Game Engine Roadmap

> A learning-focused roadmap for building a production-quality multiplayer game engine in C++

## 🎯 Project Goals

1. **Learn** - Deep understanding of game engine architecture, networking, and systems programming
2. **Build** - Create a functional multiplayer engine that can be deployed to the cloud
3. **Showcase** - Demonstrate professional engineering practices for job applications

---

## 📚 Skills You'll Demonstrate

| Category | Skills |
|----------|--------|
| **Systems Programming** | Memory management, multithreading, low-latency code |
| **Networking** | UDP/TCP, client-server architecture, state synchronization |
| **Architecture** | ECS patterns, separation of concerns, modular design |
| **DevOps** | Docker, cloud deployment, CI/CD |
| **Software Engineering** | Testing, documentation, version control |

---

## 🗺️ Development Phases

### Phase 1: Foundation Refactor (Week 1-2)
**Goal:** Restructure codebase for professional standards and multiplayer readiness

- [x] Implement Entity-Component-System (ECS) architecture
- [x] Separate game logic from rendering
- [x] Add proper logging system
- [x] Set up unit testing with doctest
- [x] Create CI/CD pipeline (GitHub Actions)

**Learning Resources:**
- [Game Programming Patterns - Component](https://gameprogrammingpatterns.com/component.html)
- [EnTT ECS Library](https://github.com/skypjack/entt) (study, then implement your own)

**Milestone:** Clean architecture where game state is independent of rendering

---

### Phase 2: Deterministic Game Loop (Week 3-4)
**Goal:** Create a fixed-timestep game loop that produces identical results given same inputs

- [x] Implement fixed timestep update loop
- [x] Separate update rate from render rate
- [x] Make all game logic deterministic (no random without seeds)
- [x] Create input recording/playback system
- [x] Add debug visualization tools (FPS Counter)

**Learning Resources:**
- [Fix Your Timestep! - Glenn Fiedler](https://gafferongames.com/post/fix_your_timestep/)
- [Game Loop Pattern](https://gameprogrammingpatterns.com/game-loop.html)

**Milestone:** Can record gameplay and replay it with identical results

---

### Phase 3: Serialization System (Week 5-6)
**Goal:** Efficiently serialize game state for network transmission

- [ ] Design binary serialization format
- [ ] Implement serialization for all game types
- [ ] Add delta compression (only send changes)
- [ ] Create snapshot system for game state
- [ ] Benchmark and optimize serialization performance

**Learning Resources:**
- [Gaffer On Games - Serialization](https://gafferongames.com/post/serialization_strategies/)
- [FlatBuffers](https://google.github.io/flatbuffers/) (study, consider using)

**Milestone:** Full game state serializes to < 1KB, delta updates < 100 bytes

---

### Phase 4: Networking Layer (Week 7-10)
**Goal:** Implement reliable UDP networking with game-specific features

- [ ] Create UDP socket wrapper (cross-platform)
- [ ] Implement connection handshake
- [ ] Add reliability layer (acknowledgments, retransmission)
- [ ] Create packet fragmentation for large messages
- [ ] Implement encryption (optional but impressive)

**Learning Resources:**
- [Gaffer On Games - Networking for Game Programmers](https://gafferongames.com/categories/game-networking/)
- [Valve's GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

**Milestone:** Two clients can connect to a server and exchange messages reliably

---

### Phase 5: Client-Server Game Architecture (Week 11-14)
**Goal:** Implement authoritative server with client prediction

- [ ] Create headless server executable
- [ ] Implement server-authoritative game logic
- [ ] Add client-side prediction
- [ ] Implement server reconciliation
- [ ] Add entity interpolation for smooth rendering
- [ ] Create lag compensation system

**Learning Resources:**
- [Source Multiplayer Networking - Valve](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)
- [Client-Side Prediction - Gabriel Gambetta](https://www.gabrielgambetta.com/client-side-prediction-server-reconciliation.html)
- [Overwatch GDC Talk](https://www.youtube.com/watch?v=W3aieHjyNvw)

**Milestone:** Playable multiplayer with smooth movement despite 100ms latency

---

### Phase 6: Cloud Deployment (Week 15-16)
**Goal:** Deploy server to cloud with proper infrastructure

- [ ] Containerize server with Docker
- [ ] Set up cloud infrastructure (AWS/GCP/DigitalOcean)
- [ ] Implement matchmaking service (basic)
- [ ] Add monitoring and logging
- [ ] Create deployment automation

**Learning Resources:**
- [Docker Documentation](https://docs.docker.com/)
- [AWS GameLift](https://aws.amazon.com/gamelift/) (study architecture)

**Milestone:** Friends can connect and play from different locations

---

### Phase 7: Polish & Portfolio (Week 17-18)
**Goal:** Make the project shine for job applications

- [ ] Write comprehensive documentation
- [ ] Create architecture diagrams
- [ ] Record demo video
- [ ] Write technical blog posts about challenges
- [ ] Add performance benchmarks
- [ ] Clean up code with consistent style

**Milestone:** Project is portfolio-ready

---

## 📁 Target Project Structure

```
Building_in_public/
├── README.md                 # Project overview
├── CONTRIBUTING.md           # How to contribute
├── LICENSE
├── CMakeLists.txt           # Root CMake
├── docs/
│   ├── ROADMAP.md           # This file
│   ├── ARCHITECTURE.md      # System design
│   ├── NETWORKING.md        # Network protocol docs
│   └── diagrams/            # Architecture diagrams
├── include/
│   ├── core/                # Core engine (ECS, logging, etc.)
│   ├── network/             # Networking layer
│   ├── game/                # Game logic
│   └── render/              # Rendering (client only)
├── src/
│   ├── core/
│   ├── network/
│   ├── game/
│   ├── render/
│   ├── client/              # Client executable
│   │   └── main_client.cpp
│   └── server/              # Server executable
│       └── main_server.cpp
├── tests/                   # Unit and integration tests
├── scripts/                 # Build and deployment scripts
├── docker/
│   └── Dockerfile.server    # Server container
└── .github/
    └── workflows/           # CI/CD pipelines
```

---

## 🔧 Technology Decisions

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **Language** | C++17 | Modern features, industry standard |
| **Build** | CMake | Cross-platform, industry standard |
| **Graphics** | OpenGL + GLFW | Already using, portable |
| **Networking** | Custom UDP | Learning experience, full control |
| **Serialization** | Custom binary | Learning, then maybe FlatBuffers |
| **Testing** | Google Test | Industry standard |
| **CI/CD** | GitHub Actions | Free, integrated |
| **Containers** | Docker | Industry standard |
| **Cloud** | DigitalOcean/AWS | Cost-effective starting point |

---

## 💼 Portfolio Presentation Tips

### What Employers Want to See

1. **Problem-solving documentation** - Write about challenges you faced
2. **Clean, readable code** - Consistent style, good naming
3. **Testing** - Shows professional mindset
4. **Architecture decisions** - Shows you think about design
5. **Performance awareness** - Benchmarks and optimization

### Technical Blog Post Ideas

- "Implementing Client-Side Prediction in C++"
- "Building a Reliable UDP Layer from Scratch"
- "Lessons Learned Building a Multiplayer Engine"
- "Delta Compression for Game State Synchronization"

---

## 📅 Weekly Log Template

```markdown
## Week N: [Title]

### Goals
- [ ] Goal 1
- [ ] Goal 2

### Completed
- Item 1
- Item 2

### Challenges
- Challenge and how I solved it

### Learnings
- Key insight

### Next Week
- Plan
```

---

## 🚀 Getting Started

Start with Phase 1. The foundation is crucial - a well-structured codebase makes everything else easier.

First concrete task: Implement a basic ECS system to replace the current Entity vector.
