#pragma once
#include <GLFW/glfw3.h>

class Renderer {
public:
    Renderer();
    ~Renderer();
    void RenderTriangle();
    void RenderMap(int rows, int cols, float cellSize);
};
