# Project Structure

```
Building_in_public/
├── .clang-format              # Code formatting rules
├── .gitignore                 # Git ignore patterns
├── CMakeLists.txt             # Build configuration
├── CMakePresets.json          # CMake presets
├── LICENSE                    # MIT License
├── README.md                  # Project overview
├── CONTRIBUTING.md            # Development guidelines
│
├── .github/
│   └── workflows/
│       └── build.yml          # CI/CD pipeline
│
├── docs/                      # Documentation
│   ├── ROADMAP.md            # Development roadmap
│   ├── ARCHITECTURE.md       # System architecture
│   ├── ENGINE_DESIGN.md      # ECS design document
│   ├── GAME_DESIGN.md        # Game design document
│   ├── DEVLOG.md             # Weekly development log
│   └── NETWORKING.md         # [Future] Network protocol docs
│
├── include/                   # Header files
│   ├── engine/               # Engine code
│   │   ├── core/             # Core engine systems
│   │   │   └── Engine.h      # Main engine class
│   │   ├── ecs/              # Entity-Component-System
│   │   │   ├── Entity.h      # Entity ID type
│   │   │   ├── Component.h   # Component type system
│   │   │   ├── World.h       # ECS world manager
│   │   │   └── System.h      # System base class
│   │   ├── platform/         # Platform abstraction
│   │   │   └── Renderer.h    # OpenGL renderer
│   │   └── systems/          # Engine systems
│   │       └── RenderSystem.h
│   │
│   ├── game/                 # Game-specific code
│   │   ├── components/       # Game components
│   │   │   └── GameComponents.h  # Transform, Renderable, etc.
│   │   └── systems/          # Game systems
│   │       └── [Future] AISystem.h, CombatSystem.h
│   │
│   └── legacy/               # Old code (to be removed)
│       ├── Entity.h          # Original entity struct
│       └── Player.h          # Original player class
│
├── src/                      # Source files (mirrors include/)
│   ├── engine/
│   │   ├── core/
│   │   │   └── Engine.cpp
│   │   ├── ecs/
│   │   │   └── World.cpp
│   │   ├── platform/
│   │   │   └── Renderer.cpp
│   │   └── systems/
│   │       └── RenderSystem.cpp
│   │
│   ├── game/
│   │   └── systems/
│   │       └── [Future] AISystem.cpp
│   │
│   ├── client/               # [Future] Client executable
│   │   └── main_client.cpp
│   │
│   ├── server/               # [Future] Server executable
│   │   └── main_server.cpp
│   │
│   └── main.cpp              # Current main (temporary)
│
├── tests/                    # Unit and integration tests
│   └── [Future] test_ecs.cpp
│
├── scripts/                  # Build and deployment scripts
│   ├── build.sh             # Build script
│   └── clean.sh             # Clean script
│
└── build/                    # Build output (gitignored)
    ├── client               # Client executable
    ├── main                 # Main executable
    └── ...
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
| **ECS Core** | ✅ Done | Entity.h, Component.h, World.h/cpp, System.h |
| **Render System** | ✅ Done | RenderSystem.h/cpp, Renderer.h/cpp |
| **Game Components** | ✅ Basic | Transform, Renderable |
| **Input System** | ⏳ Next | - |
| **Physics/Movement** | ⏳ Planned | - |
| **AI System** | ⏳ Planned | - |
| **Combat System** | ⏳ Planned | - |
| **Networking** | ⏳ Planned | - |

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

1. Remove test ECS code from main.cpp
2. Implement InputSystem for player movement
3. Add Velocity component and MovementSystem
4. Create proper client/main_client.cpp
5. Start on game logic (dungeon, enemies)
