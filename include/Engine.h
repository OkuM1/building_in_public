#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

#include "Renderer.h"
#include "Entity.h"
#include "Entity.h"

class Engine {
public:
    Engine(int width, int height, const std::string& title);
    ~Engine();
    void Run();

private:
    GLFWwindow* window;
    void Init();
    void MainLoop();
    void Cleanup();
    void Update();
    void Render();
    int width, height;
    std::string title;
    Renderer renderer;
    std::vector<Entity> entities;
    // Key state tracking for discrete movement
    bool lastUp, lastDown, lastLeft, lastRight;
};
