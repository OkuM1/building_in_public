
#pragma once
#include <GLFW/glfw3.h>
#include "Entity.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    void RenderTriangle();
    void RenderMap(int rows, int cols, float cellSize);
    void RenderEntity(const Entity& entity, float cellSize, int gridCols, int gridRows);
};
