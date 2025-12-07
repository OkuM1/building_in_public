#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <memory>

#include "engine/platform/Renderer.h"
#include "engine/ecs/World.h"
#include "engine/ecs/System.h"
#include "game/components/GameComponents.h"

class Engine {
public:
    Engine(int width, int height, const std::string& title);
    ~Engine();
    void Run();

    // Access to ECS world
    engine::World& GetWorld() { return world; }

private:
    GLFWwindow* window;
    void Init();
    void InitECS();
    void CreateTestEntities();
    void MainLoop();
    void Cleanup();
    void Update(float dt);
    void Render(float alpha);
    int width, height;
    std::string title;
    Renderer renderer;
    
    // Game Loop Timing
    const float TARGET_FPS = 60.0f;
    const float FIXED_TIMESTEP = 1.0f / TARGET_FPS;
    float accumulator = 0.0f;
    float lastFrameTime = 0.0f;
    
    // Debug / FPS
    int frameCount = 0;
    float fpsTimer = 0.0f;

    // ECS World
    engine::World world;
    
    // ECS Systems
    std::vector<std::unique_ptr<engine::System>> systems;
};
