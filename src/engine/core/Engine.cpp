#include "engine/core/Engine.h"
#include "engine/systems/RenderSystem.h"
#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
#include "engine/core/Logger.h"

Engine::Engine(int width, int height, const std::string& title)
    : window(nullptr), width(width), height(height), title(title) {
    Init();
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        engine::Logger::Error("Failed to create GLFW window");
        glfwTerminate();
    } else {
        glfwMakeContextCurrent(window);
    }
    
    // Initialize ECS
    InitECS();
    CreateTestEntities();
}

void Engine::InitECS() {
    // Register all component types
    world.registerComponent<game::Transform>();
    world.registerComponent<game::PreviousTransform>();
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
    world.addComponent(player, game::PreviousTransform{0.0f, 0.0f, 0.0f});
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
    world.addComponent(enemy, game::PreviousTransform{0.3f, 0.2f, 0.0f});
    world.addComponent(enemy, game::Renderable{
        game::Renderable::Shape::Circle,
        0.8f, 0.2f, 0.2f,  // Red
        0.05f, 0.05f,       // Size
        5                   // Layer
    });
    world.addComponent(enemy, game::Enemy{});
    
    engine::Logger::Info("Created player entity (ID: ", player, ") - Use WASD/Arrows to move!");
    engine::Logger::Info("Created enemy entity (ID: ", enemy, ")");
}

Engine::~Engine() {
    Cleanup();
}

void Engine::Init() {
    if (!glfwInit()) {
        engine::Logger::Error("Failed to initialize GLFW");
    }
}

void Engine::MainLoop() {
    lastFrameTime = glfwGetTime();

    while (window && !glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        float frameTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Prevent spiral of death (cap frame time)
        if (frameTime > 0.25f) frameTime = 0.25f;

        accumulator += frameTime;

        while (accumulator >= FIXED_TIMESTEP) {
            Update(FIXED_TIMESTEP);
            accumulator -= FIXED_TIMESTEP;
        }

        // Calculate alpha for interpolation
        float alpha = accumulator / FIXED_TIMESTEP;
        Render(alpha);
        glfwSwapBuffers(window);
        glfwPollEvents();

        // FPS Counter
        frameCount++;
        fpsTimer += frameTime;
        if (fpsTimer >= 1.0f) {
            std::string newTitle = title + " - " + std::to_string(frameCount) + " FPS";
            glfwSetWindowTitle(window, newTitle.c_str());
            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }
}

void Engine::Update(float dt) {
    if (window) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }
    
    // Run ECS systems (except render system)
    for (auto& system : systems) {
        // Skip render system here (it runs in Render())
        if (dynamic_cast<engine::RenderSystem*>(system.get()) == nullptr) {
            system->update(world, dt);
        }
    }
}

void Engine::Render(float alpha) {
    // Run only the render system
    for (auto& system : systems) {
        if (dynamic_cast<engine::RenderSystem*>(system.get()) != nullptr) {
            system->update(world, alpha);
        }
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
