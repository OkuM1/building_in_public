#include "Renderer.h"

Renderer::Renderer() {}
Renderer::~Renderer() {}


void Renderer::RenderTriangle() {
    float vertices[] = {
        0.0f,  0.5f, 0.0f,
       -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void Renderer::RenderMap(int rows, int cols, float cellSize) {
    glColor3f(0.8f, 0.8f, 0.8f); // Light gray for grid
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = (c - cols / 2) * cellSize;
            float y = (r - rows / 2) * cellSize;
            float half = cellSize / 2.0f;
            float rect[8] = {
                x - half, y - half,
                x + half, y - half,
                x + half, y + half,
                x - half, y + half
            };
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(2, GL_FLOAT, 0, rect);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            glDisableClientState(GL_VERTEX_ARRAY);
        }
    }
}
