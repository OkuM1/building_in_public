#include "Engine.h"
#include <iostream>

Engine::Engine(int width, int height, const std::string& title)
    : window(nullptr), width(width), height(height), title(title), player(0, 0, 10) {
    Init();
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    } else {
        glfwMakeContextCurrent(window);
    }
}

Engine::~Engine() {
    Cleanup();
}

void Engine::Init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
    }
}

// Simple triangle vertex data
static float vertices[] = {
    0.0f,  0.5f, 0.0f,
   -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f
};

void Engine::MainLoop() {
    while (window && !glfwWindowShouldClose(window)) {
        Update();
        Render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Engine::Update() {
    if (window) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        // Arrow key movement
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            player.y = std::max(player.y - 1, 0);
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            player.y = std::min(player.y + 1, 9);
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            player.x = std::max(player.x - 1, 0);
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            player.x = std::min(player.x + 1, 15);
        }
    }
}

void Engine::Render() {
    renderer.RenderMap(10, 16, 0.1f); // Example grid size and cell size
    // Draw player as a colored rectangle
    glColor3f(0.2f, 0.8f, 0.2f); // Green
    float cellSize = 0.1f;
    float x = (player.x - 16 / 2) * cellSize;
    float y = (player.y - 10 / 2) * cellSize;
    float half = cellSize / 2.0f;
    float rect[8] = {
        x - half, y - half,
        x + half, y - half,
        x + half, y + half,
        x - half, y + half
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, rect);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void Engine::Run() {
    if (window) 
    {
        MainLoop();
    }
}

void Engine::Cleanup() {
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}
