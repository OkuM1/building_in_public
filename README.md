
## Features
- Grid-based map rendering
- Player entity with position and movement
- Arrow key input for movement
- Minimalist graphics (rectangles for map and player)

## Getting Started

### Prerequisites
- C++ compiler (g++)
- CMake
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
include/      # Header files (Engine, Renderer, Player)
src/          # Source files (Engine.cpp, Renderer.cpp, main.cpp)
CMakeLists.txt
build.sh      # Build script
README.md     # This file
```

## How the Engine Works
- The window displays a grid map.
- The player is a green rectangle that moves with arrow keys.
- Movement is clamped to the grid boundaries.

## Learning Resources
- [LearnOpenGL](https://learnopengl.com/)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)
- [C++ Reference](https://en.cppreference.com/)
- [Game Programming Patterns](https://gameprogrammingpatterns.com/)
