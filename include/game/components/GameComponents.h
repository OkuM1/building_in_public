#pragma once

namespace game {

// ============================================================================
// Core Components
// ============================================================================

struct Transform {
    float x, y;          // World position
    float rotation;      // Rotation in radians
};

struct PreviousTransform {
    float x, y;          // Position at start of frame
    float rotation;
};

struct Velocity {
    float vx, vy;        // Velocity per second
};

struct Renderable {
    enum class Shape { Rectangle, Circle };
    Shape shape = Shape::Rectangle;
    float r, g, b;       // Color (RGB 0-1)
    float width, height; // Size
    int layer = 0;       // Draw order (higher = on top)
};

// ============================================================================
// Input Components
// ============================================================================

struct PlayerInput {
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool attack = false;
    bool dodge = false;
};

// ============================================================================
// Tag Components (no data, just markers)
// ============================================================================

struct Player {};        // Marks entity as player-controlled
struct Enemy {};         // Marks entity as enemy

} // namespace game
