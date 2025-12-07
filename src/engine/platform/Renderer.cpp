#include "engine/platform/Renderer.h"
#include <GLFW/glfw3.h>
#include <cmath>

Renderer::Renderer() {}
Renderer::~Renderer() {}

// ============================================================================
// New ECS-friendly rendering methods
// ============================================================================

void Renderer::Clear() {
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::RenderRectangle(float x, float y, float width, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    float hw = width / 2.0f;
    float hh = height / 2.0f;
    float rect[8] = {
        x - hw, y - hh,
        x + hw, y - hh,
        x + hw, y + hh,
        x - hw, y + hh
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, rect);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void Renderer::RenderCircle(float x, float y, float radius, float r, float g, float b) {
    glColor3f(r, g, b);
    const int segments = 32;
    float vertices[segments * 2];
    
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * 3.14159f * i / segments;
        vertices[i * 2] = x + radius * cosf(angle);
        vertices[i * 2 + 1] = y + radius * sinf(angle);
    }
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments);
    glDisableClientState(GL_VERTEX_ARRAY);
}
