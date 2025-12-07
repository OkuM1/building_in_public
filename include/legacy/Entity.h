#pragma once
#include <string>

struct Entity {
    int x, y; // Grid position
    std::string tag; // Shape identifier (e.g., "A", "B", ...)
    Entity(int startX, int startY, const std::string& tag = "") : x(startX), y(startY), tag(tag) {}
};
