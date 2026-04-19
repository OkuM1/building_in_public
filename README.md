# Networked C++ Game Engine

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

> A from-scratch, highly performant networked game engine in C++. The engine
> is the product; games are testbeds.

## 🎯 Project

This is a learning-focused project to build a production-quality multiplayer
game engine: custom ECS, deterministic fixed-timestep simulation, custom UDP
networking with client-side prediction and server reconciliation, and
cloud-deployed authoritative servers.

Performance is the success criterion. Targets live in
[docs/ROADMAP.md](docs/ROADMAP.md) and measured numbers will be tracked in
`bench/results.md` as the engine grows.

## 🕹️ Testbeds

- **[Sumo Arena](docs/SUMO_ARENA.md)** — minimalist 4–16 player physics PvP.
  Chosen to force every hard netcode problem (prediction, reconciliation,
  lag compensation, interest management) with zero art budget.
- **[Dungeon Crawler](docs/GAME_DESIGN.md)** — 1–4 player co-op roguelite,
  parked as a later showcase on the same engine.

## 📖 Documentation

- [**Development Guide**](docs/DEVELOPMENT.md) — **start here** when resuming work; living context doc
- [**Code Documentation Conventions**](docs/CODE_DOCS.md) — header-comment style, design notes, learning notes
- [**Design notes**](docs/design/) — ADR-style records of every non-trivial decision
- [**Learning notes**](docs/learning/) — per-commit reflections on clean-code principles
- [**Roadmap**](docs/ROADMAP.md) — engine-first phases with performance targets
- [**Sumo Arena**](docs/SUMO_ARENA.md) — primary testbed design
- [**Dungeon Crawler**](docs/GAME_DESIGN.md) — later showcase game design
- [**Architecture**](docs/ARCHITECTURE.md) — technical design and system overview
- [**Engine Design**](docs/ENGINE_DESIGN.md) — ECS layering and patterns
- [**Devlog**](docs/DEVLOG.md) — weekly progress log
- [**Contributing**](CONTRIBUTING.md) — code style and development practices

## Features

- Entity-Component-System (ECS) architecture (`engine::World`, packed component arrays)
- Fixed 60 Hz deterministic update loop with render-time interpolation
- Input recording & playback (proves determinism)
- Thread-safe, color-coded logger
- Doctest-based unit tests + GitHub Actions CI
- Minimalist OpenGL/GLFW renderer (rectangles and circles)

## Getting Started

### Prerequisites
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- OpenGL development libraries
- GLFW development libraries

### Build Instructions

```bash
# Clone and build
git clone https://github.com/OkuM1/building_in_public.git
cd building_in_public
./build.sh

# Run
./build/main

# Run tests
cd build && ctest --output-on-failure
```

### Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move player |
| F5 | Start input recording |
| F6 | Stop recording |
| F7 | Replay recording (deterministic) |
| ESC | Quit |

## Project Structure

```
include/
├── engine/            # Reusable engine code
│   ├── core/          # Engine, Logger, InputRecorder
│   ├── ecs/           # Entity-Component-System
│   ├── platform/      # OpenGL / GLFW wrappers
│   └── systems/       # Render, Input, Movement systems
└── game/              # Game-specific code
    ├── components/    # Transform, Velocity, Renderable, PlayerInput, ...
    └── systems/       # (reserved: AI, Combat)
src/                   # Mirrors include/ plus src/main.cpp
tests/                 # doctest unit tests
docs/                  # Design docs, devlog, roadmap
```

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for the authoritative, current layout.

## How the Engine Works

- `engine::World` owns entities and packed component arrays.
- Each frame runs a **fixed 60 Hz** simulation step (`Update`) and then renders
  with interpolation between the previous and current transforms for smoothness
  decoupled from the physics tick.
- Systems (`RenderSystem`, `InputSystem`, `MovementSystem`) are composed in
  `Engine` and operate on component sets.

## 🗺️ Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| 1    | Foundation & ECS                  | ✅ Done    |
| 2    | Deterministic simulation          | ✅ Done    |
| 2.5  | Engine / Client split (headless-ready) | 🔜 Next   |
| 3    | Serialization & snapshots         | ⏳ Planned |
| 4    | Reliable UDP layer                | ⏳ Planned |
| 5    | Replication (prediction, lag comp, AoI) | ⏳ Planned |
| 5.5  | Rollback netcode *(optional)*     | ⏳ Stretch |
| 6    | Deployment & ops                  | ⏳ Planned |
| 7    | Benchmarks, docs, demo            | ⏳ Planned |

See [ROADMAP.md](docs/ROADMAP.md) for milestones and performance targets, and
[DEVELOPMENT.md](docs/DEVELOPMENT.md) for the current in-flight work.

## Learning Resources
- [LearnOpenGL](https://learnopengl.com/)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)
- [Gaffer On Games - Networking](https://gafferongames.com/)
- [Game Programming Patterns](https://gameprogrammingpatterns.com/)
- [Valve Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)

## License

MIT License - see LICENSE file for details
