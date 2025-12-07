#include "engine/core/Engine.h"
#include "engine/systems/RenderSystem.h"
#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
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
    
    // Initialize ECS
    InitECS();
    CreateTestEntities();
    
    // Add legacy entities (will be removed once fully migrated)
    entities.push_back(Entity(0, 0, "A"));
    entities.push_back(Entity(5, 5, "B"));
    entities.push_back(Entity(10, 8, "C"));
}

void Engine::InitECS() {
    // Register all component types
    world.registerComponent<game::Transform>();
    world.registerComponent<game::Renderable>();
    world.registerComponent<game::Velocity>();
    world.registerComponent<game::PlayerInput>();
    world.registerComponent<game::Player>();
    world.registerComponent<game::Enemy>();
    
    // Add systems (order matters!)
    systems.push_back(std::make_unique<engine::InputSystem>(window));
    systems.push_back(std::make_unique<engine::MovementSystem>());
    systems.push_back(std::make_unique<engine::RenderSystem>(renderer));
}

void Engine::CreateTestEntities() {
    // Create player entity (controllable)
    engine::EntityId player = world.createEntity();
    world.addComponent(player, game::Transform{0.0f, 0.0f, 0.0f});
    world.addComponent(player, game::Velocity{0.0f, 0.0f});
    world.addComponent(player, game::PlayerInput{});
    world.addComponent(player, game::Renderable{
        game::Renderable::Shape::Rectangle,
        0.2f, 0.8f, 0.2f,  // Green
        0.08f, 0.08f,       // Size
        10                  // Layer
    });
    world.addComponent(player, game::Player{});
    
    // Create enemy entity (not controllable)
    engine::EntityId enemy = world.createEntity();
    world.addComponent(enemy, game::Transform{0.3f, 0.2f, 0.0f});
    world.addComponent(enemy, game::Renderable{
        game::Renderable::Shape::Circle,
        0.8f, 0.2f, 0.2f,  // Red
        0.05f, 0.05f,       // Size
        5                   // Layer
    });
    world.addComponent(enemy, game::Enemy{});
    
    std::cout << "Created player entity (ID: " << player << ") - Use WASD/Arrows to move!" << std::endl;
    std::cout << "Created enemy entity (ID: " << enemy << ")" << std::endl;
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
    }
    
    // Run ECS systems (except render system)
    // Fixed timestep of 1/60 for now (will improve later)
    const float dt = 1.0f / 60.0f;
    
    for (auto& system : systems) {
        // Skip render system here (it runs in Render())
        if (dynamic_cast<engine::RenderSystem*>(system.get()) == nullptr) {
            system->update(world, dt);
        }
    }
    
    // Legacy entity update (will be removed)
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
    // Run only the render system
    for (auto& system : systems) {
        if (dynamic_cast<engine::RenderSystem*>(system.get()) != nullptr) {
            system->update(world, 0.0f);
        }
    }
    
    // Legacy rendering (will be removed)
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
