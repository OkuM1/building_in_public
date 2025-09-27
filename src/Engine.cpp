#include "Engine.h"
#include <iostream>

Engine::Engine(int width, int height, const std::string& title)
    : window(nullptr), width(width), height(height), title(title) {
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
    }
}

void Engine::Render() {
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
