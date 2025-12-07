# Dungeon Crawler - Multiplayer Co-op Engine

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

> A cooperative multiplayer dungeon crawler for 1-4 players, built in C++ as a learning project.

## 🎮 The Game

Team up with friends to explore procedurally generated dungeons, fight monsters, collect loot, and survive as deep as you can!

**Genre:** Top-down co-op action roguelite  
**Players:** 1-4 online  
**Inspirations:** Enter the Gungeon, Gauntlet, Nuclear Throne

## 🎯 Project Goals

This project is a journey to build a multiplayer game from scratch, focusing on:

- **Game Engine** - ECS architecture, game loop, rendering
- **Networking** - Client-server, prediction, lag compensation  
- **Gameplay** - Combat, AI, procedural generation, loot
- **Cloud Deployment** - Containerized game servers
- **Professional Practices** - Testing, documentation, CI/CD

## 📖 Documentation

- [**Game Design**](docs/GAME_DESIGN.md) - Full game design document
- [**Roadmap**](docs/ROADMAP.md) - Development phases and learning resources
- [**Architecture**](docs/ARCHITECTURE.md) - Technical design and system overview
- [**Contributing**](CONTRIBUTING.md) - Code style and development practices

## Features
- Grid-based map rendering
- Player entity with position and movement
- Arrow key input for movement
- Minimalist graphics (rectangles for map and player)

## Getting Started

### Prerequisites
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- OpenGL development libraries
- GLFW development libraries

### Build Instructions
1. Clone the repository
2. Run the build script:
	```bash
	./build.sh
	```
3. Run the executable:
	```bash
	cd build
	./main
	```

## Project Structure
```
include/           # Header files
├── core/          # Core engine (ECS, logging) [coming soon]
├── network/       # Networking layer [coming soon]
├── game/          # Game logic [coming soon]
└── render/        # Rendering
src/               # Source files
├── client/        # Client executable [coming soon]
├── server/        # Server executable [coming soon]
└── ...
docs/              # Documentation
tests/             # Unit and integration tests [coming soon]
```

## How the Engine Works
- The window displays a grid map.
- The player is a green rectangle that moves with arrow keys.
- Movement is clamped to the grid boundaries.

## 🗺️ Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Foundation & ECS Architecture | 🔄 In Progress |
| 2 | Deterministic Game Loop | ⏳ Planned |
| 3 | Serialization System | ⏳ Planned |
| 4 | Networking Layer | ⏳ Planned |
| 5 | Client-Server Architecture | ⏳ Planned |
| 6 | Cloud Deployment | ⏳ Planned |
| 7 | Polish & Portfolio | ⏳ Planned |

See [ROADMAP.md](docs/ROADMAP.md) for details.

## Learning Resources
- [LearnOpenGL](https://learnopengl.com/)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)
- [Gaffer On Games - Networking](https://gafferongames.com/)
- [Game Programming Patterns](https://gameprogrammingpatterns.com/)
- [Valve Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)

## License

MIT License - see LICENSE file for details
