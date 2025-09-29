#include "Engine.h"
#include <iostream>

Engine::Engine(int width, int height, const std::string& title)
    : window(nullptr), width(width), height(height), title(title), lastUp(false), lastDown(false), lastLeft(false), lastRight(false) {
    Init();
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    } else {
        glfwMakeContextCurrent(window);
    }
    // Add generic shapes to entities
    entities.push_back(Entity(0, 0, "A"));
    entities.push_back(Entity(5, 5, "B"));
    entities.push_back(Entity(10, 8, "C"));
}

Engine::~Engine() {
    Cleanup();
}

void Engine::Init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
    }
}

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
        // Move shape "A" with arrow keys (discrete movement)
        for (auto& entity : entities) {
            if (entity.tag == "A") {
                bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
                bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
                bool leftPressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
                bool rightPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;

                if (upPressed && !lastUp) {
                    entity.y = std::max(entity.y - 1, 0);
                }
                if (downPressed && !lastDown) {
                    entity.y = std::min(entity.y + 1, 9);
                }
                if (leftPressed && !lastLeft) {
                    entity.x = std::max(entity.x - 1, 0);
                }
                if (rightPressed && !lastRight) {
                    entity.x = std::min(entity.x + 1, 15);
                }

                lastUp = upPressed;
                lastDown = downPressed;
                lastLeft = leftPressed;
                lastRight = rightPressed;
            }
        }
    }
}

namespace {
    constexpr int GRID_ROWS = 10;
    constexpr int GRID_COLS = 16;
    constexpr float CELL_SIZE = 0.1f;

    void setEntityColor(const std::string& tag) {
        if (tag == "A") glColor3f(0.2f, 0.8f, 0.2f); // Green
        else if (tag == "B") glColor3f(0.2f, 0.2f, 0.8f); // Blue
        else if (tag == "C") glColor3f(0.8f, 0.2f, 0.2f); // Red
        else glColor3f(0.8f, 0.8f, 0.8f); // Default gray
    }
}

void Engine::Render() {
    renderer.RenderMap(GRID_ROWS, GRID_COLS, CELL_SIZE);
    for (const auto& entity : entities) {
        setEntityColor(entity.tag);
        renderer.RenderEntity(entity, CELL_SIZE, GRID_COLS, GRID_ROWS);
    }
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
