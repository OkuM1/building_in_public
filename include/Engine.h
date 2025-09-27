#pragma once
#include <GLFW/glfw3.h>
#include <string>

#include "Renderer.h"
#include "Player.h"

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
    Player player;
};
