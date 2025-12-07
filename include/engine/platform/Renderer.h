
#pragma once
#include <GLFW/glfw3.h>

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // New ECS-friendly rendering methods
    void Clear();
    void RenderRectangle(float x, float y, float width, float height, float r, float g, float b);
    void RenderCircle(float x, float y, float radius, float r, float g, float b);
};
