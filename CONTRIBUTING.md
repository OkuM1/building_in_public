# Contributing to the Multiplayer Engine

Thank you for your interest in contributing! This document outlines our development practices.

## Development Setup

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- OpenGL development libraries
- GLFW3

### Building

```bash
# Clone the repository
git clone https://github.com/OkuM1/building_in_public.git
cd building_in_public

# Build
./build.sh

# Run
./build/main
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## Code Style

### General Guidelines

- Use **C++17** features where appropriate
- Prefer `const` correctness
- Use `#pragma once` for header guards
- Keep functions short and focused
- Document public APIs

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `NetworkManager` |
| Functions | camelCase | `processInput()` |
| Variables | camelCase | `playerCount` |
| Constants | SCREAMING_SNAKE | `MAX_PLAYERS` |
| Members | camelCase with m_ prefix (optional) | `m_socket` or `socket` |
| Namespaces | lowercase | `network::` |

### Example

```cpp
namespace engine {

class NetworkManager {
public:
    static constexpr int MAX_CLIENTS = 16;
    
    NetworkManager();
    ~NetworkManager();
    
    // Attempts to connect to a server
    // Returns true on success
    bool connect(const std::string& address, uint16_t port);
    
    void disconnect();
    bool isConnected() const { return connected; }

private:
    bool connected = false;
    Socket socket;
};

} // namespace engine
```

### Formatting

- Indentation: 4 spaces (no tabs)
- Braces: Same line for functions, next line for classes
- Line length: 100 characters max
- Use clang-format with the provided `.clang-format` file

## Git Workflow

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `refactor/description` - Code refactoring
- `docs/description` - Documentation updates

### Commit Messages

Follow conventional commits:

```
type(scope): description

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

Examples:
```
feat(network): add UDP socket wrapper
fix(renderer): correct entity interpolation
docs(readme): update build instructions
refactor(ecs): improve component storage layout
```

### Pull Request Process

1. Create a feature branch from `master`
2. Make your changes with clear commits
3. Ensure all tests pass
4. Update documentation if needed
5. Create a PR with a clear description
6. Address review feedback

## Architecture Guidelines

### Adding New Systems

1. Create header in `include/systems/`
2. Create implementation in `src/systems/`
3. Register system in the appropriate manager
4. Add tests in `tests/`

### Adding Network Messages

1. Define message type in `include/network/PacketTypes.h`
2. Implement serialization in the message struct
3. Add handler in client/server as appropriate
4. Document in `docs/NETWORKING.md`

### Performance Considerations

- Profile before optimizing
- Prefer data-oriented design for hot paths
- Minimize allocations in the game loop
- Use object pools for frequently created/destroyed objects

## Testing

### Test Categories

- **Unit tests**: Test individual components in isolation
- **Integration tests**: Test component interactions
- **Network tests**: Test client-server communication

### Writing Tests

```cpp
#include <gtest/gtest.h>
#include "game/GameState.h"

TEST(GameStateTest, SerializationRoundTrip) {
    GameState original;
    original.addEntity(1, 10.0f, 20.0f);
    
    auto serialized = original.serialize();
    GameState deserialized = GameState::deserialize(serialized);
    
    EXPECT_EQ(original, deserialized);
}
```

## Documentation

- Document all public APIs with comments
- Update `docs/` for architectural changes
- Include examples for complex features
- Keep README.md up to date

## Questions?

Open an issue for any questions about contributing!
