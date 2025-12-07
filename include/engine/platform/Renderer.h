
#pragma once
#include <GLFW/glfw3.h>
#include "legacy/Entity.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    void RenderTriangle();
    void RenderMap(int rows, int cols, float cellSize);
    void RenderEntity(const Entity& entity, float cellSize, int gridCols, int gridRows);
    
    // New ECS-friendly rendering methods
    void Clear();
    void RenderRectangle(float x, float y, float width, float height, float r, float g, float b);
    void RenderCircle(float x, float y, float radius, float r, float g, float b);
};
