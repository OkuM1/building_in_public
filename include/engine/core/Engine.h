#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <memory>

#include "engine/platform/Renderer.h"
#include "legacy/Entity.h" // Old Entity struct
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
    void Update();
    void Render();
    int width, height;
    std::string title;
    Renderer renderer;
    
    // Legacy entities (to be removed)
    std::vector<Entity> entities;
    
    // ECS World
    engine::World world;
    
    // ECS Systems
    std::vector<std::unique_ptr<engine::System>> systems;

    // Key state tracking for discrete movement
    bool lastUp, lastDown, lastLeft, lastRight;
};
