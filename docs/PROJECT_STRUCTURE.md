# Project Structure

> See [DEVELOPMENT.md](DEVELOPMENT.md) for the authoritative current layout.
> This file describes the **target** structure; items marked *(planned)* do not
> yet exist on disk.

```
Building_in_public/
├── .clang-format              # Code formatting rules
├── .gitignore                 # Git ignore patterns
├── CMakeLists.txt             # Build configuration
├── CMakePresets.json          # CMake presets
├── LICENSE                    # MIT License
├── README.md                  # Project overview
├── CONTRIBUTING.md            # Development guidelines
├── build.sh  clean.sh         # Build / clean scripts (repo root)
│
├── .github/
│   └── workflows/
│       └── build.yml          # CI/CD pipeline
│
├── docs/                      # Documentation
│   ├── DEVELOPMENT.md         # Living context doc (start here)
│   ├── ROADMAP.md             # Development roadmap
│   ├── ARCHITECTURE.md        # System architecture
│   ├── ENGINE_DESIGN.md       # ECS design document
│   ├── GAME_DESIGN.md         # Game design document
│   ├── DEVLOG.md              # Weekly development log
│   └── NETWORKING.md          # (planned) Network protocol docs
│
├── include/                   # Header files
│   ├── engine/                # Engine code
│   │   ├── core/              # Engine, Logger, InputRecorder
│   │   ├── ecs/               # Entity, Component, World, System
│   │   ├── platform/          # Renderer (GLFW + OpenGL)
│   │   └── systems/           # RenderSystem, InputSystem, MovementSystem
│   └── game/                  # Game-specific code
│       ├── components/        # GameComponents.h (Transform, Velocity, …)
│       └── systems/           # (planned) AISystem.h, CombatSystem.h
│
├── src/                       # Source files (mirrors include/)
│   ├── main.cpp               # Current entry point
│   ├── engine/
│   │   ├── core/              # Engine.cpp, Logger.cpp, InputRecorder.cpp
│   │   ├── ecs/               # World.cpp
│   │   ├── platform/          # Renderer.cpp
│   │   └── systems/           # RenderSystem.cpp, InputSystem.cpp, MovementSystem.cpp
│   └── game/
│       └── systems/           # (empty — reserved)
│   # Planned: src/client/main_client.cpp, src/server/main_server.cpp
│
├── tests/                     # Unit and integration tests
│   ├── main.cpp               # doctest runner
│   └── test_logger.cpp
│
└── build/                     # Build output (gitignored)
    ├── main                   # Game executable
    ├── client                 # Alias target, identical to main for now
    └── unit_tests             # Doctest runner
```

## Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         GAME LAYER                           │
│  game/components/  game/systems/  game/                      │
│  - GameComponents  - AISystem    - DungeonGenerator          │
│  - Combat data     - CombatSystem                            │
└─────────────────────────────────────────────────────────────┘
                          ▼ depends on
┌─────────────────────────────────────────────────────────────┐
│                        ENGINE LAYER                          │
│  engine/ecs/       engine/systems/    engine/core/           │
│  - Entity          - RenderSystem     - Engine               │
│  - Component       - [InputSystem]    - [Logger]             │
│  - World           - [PhysicsSystem]                         │
│  - System                                                    │
└─────────────────────────────────────────────────────────────┘
                          ▼ depends on
┌─────────────────────────────────────────────────────────────┐
│                       PLATFORM LAYER                         │
│  engine/platform/                                            │
│  - Renderer (OpenGL/GLFW wrapper)                            │
│  - [Input] (Keyboard/Mouse)                                  │
│  - [Network] (Sockets)                                       │
└─────────────────────────────────────────────────────────────┘
```

## Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `RenderSystem`, `NetworkManager` |
| Functions | camelCase | `update()`, `createEntity()` |
| Variables | camelCase | `entityId`, `playerCount` |
| Constants | SCREAMING_SNAKE | `MAX_ENTITIES`, `TICK_RATE` |
| Namespaces | lowercase | `engine`, `game` |
| Files | PascalCase matching class | `RenderSystem.h`, `RenderSystem.cpp` |

## File Organization Rules

1. **Header location matches namespace**: `engine::World` → `include/engine/ecs/World.h`
2. **Implementation mirrors header**: `include/X/Y.h` → `src/X/Y.cpp`
3. **One class per file** (except small related structs)
4. **Include guards**: Use `#pragma once`
5. **Include order**:
   ```cpp
   // 1. Corresponding header (for .cpp files)
   #include "MyClass.h"
   
   // 2. Engine headers
   #include "engine/ecs/World.h"
   
   // 3. Game headers
   #include "game/components/GameComponents.h"
   
   // 4. Standard library
   #include <vector>
   #include <memory>
   ```

## Current Status

| Component | Status | Files |
|-----------|--------|-------|
| **ECS Core**        | ✅ Done  | Entity.h, Component.h, World.h/cpp, System.h |
| **Logger**          | ✅ Done  | Logger.h/cpp |
| **Renderer**        | ✅ Done  | Renderer.h/cpp |
| **Render System**   | ✅ Done  | RenderSystem.h/cpp (with interpolation) |
| **Input System**    | ✅ Done  | InputSystem.h/cpp |
| **Movement System** | ✅ Done  | MovementSystem.h/cpp |
| **Fixed Timestep Loop** | ✅ Done | Engine.cpp |
| **Input Recorder**  | ✅ Done  | InputRecorder.h/cpp |
| **Game Components** | ✅ Basic | Transform, PreviousTransform, Velocity, Renderable, PlayerInput, Player/Enemy tags |
| **Collision System**| 🔜 Next  | — |
| **AI System**       | ⏳ Planned | — |
| **Combat System**   | ⏳ Planned | — |
| **Serialization**   | ⏳ Planned | — |
| **Networking**      | ⏳ Planned | — |

## Build Targets

```bash
# Build everything
cd build && cmake .. && make

# Run client (current main)
./main

# [Future] Run server
./server

# [Future] Run tests
ctest
```

## Next Steps

See [DEVELOPMENT.md → Next up](DEVELOPMENT.md#-next-up-ordered) for the current
ordered task list. In summary:

1. Add `CollisionSystem` (AABB) and a `Collider` component.
2. Add `AISystem` (enemy pursues player).
3. Add `Health` + `CombatSystem`.
4. Then begin Phase 3: binary serialization for snapshots.
