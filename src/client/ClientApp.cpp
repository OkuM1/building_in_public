#include "client/ClientApp.h"

#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
#include "client/render/Rendering.h"
#include "engine/core/Logger.h"

#include <memory>

/**
 * @file ClientApp.cpp
 * @brief Implementation of @ref client::ClientApp.
 * @see docs/design/0005-introduce-clientapp.md
 */

namespace client {

ClientApp::ClientApp(int width, int height, const std::string& title)
    : width_(width), height_(height), title_(title) {
    InitWindow();
    InitSimulation();
    CreateTestEntities();
}

ClientApp::~ClientApp() {
    Cleanup();
}

void ClientApp::InitWindow() {
    if (!glfwInit()) {
        engine::Logger::Error("Failed to initialize GLFW");
        return;
    }
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        engine::Logger::Error("Failed to create GLFW window");
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window_);
}

void ClientApp::InitSimulation() {
    auto& world = sim_.world();
    world.registerComponent<game::Transform>();
    world.registerComponent<game::PreviousTransform>();
    world.registerComponent<game::Renderable>();
    world.registerComponent<game::Velocity>();
    world.registerComponent<game::PlayerInput>();
    world.registerComponent<game::Player>();
    world.registerComponent<game::Enemy>();

    sim_.addSystem(std::make_unique<engine::InputSystem>());
    sim_.addSystem(std::make_unique<engine::MovementSystem>());
}

void ClientApp::CreateTestEntities() {
    auto& world = sim_.world();

    playerEntity_ = world.createEntity();
    world.addComponent(playerEntity_, game::Transform{0.0f, 0.0f, 0.0f});
    world.addComponent(playerEntity_, game::PreviousTransform{0.0f, 0.0f, 0.0f});
    world.addComponent(playerEntity_, game::Velocity{0.0f, 0.0f});
    world.addComponent(playerEntity_, game::PlayerInput{});
    world.addComponent(playerEntity_, game::Renderable{
        game::Renderable::Shape::Rectangle,
        0.2f, 0.8f, 0.2f,
        0.08f, 0.08f,
        10
    });
    world.addComponent(playerEntity_, game::Player{});

    engine::EntityId enemy = world.createEntity();
    world.addComponent(enemy, game::Transform{0.3f, 0.2f, 0.0f});
    world.addComponent(enemy, game::PreviousTransform{0.3f, 0.2f, 0.0f});
    world.addComponent(enemy, game::Renderable{
        game::Renderable::Shape::Circle,
        0.8f, 0.2f, 0.2f,
        0.05f, 0.05f,
        5
    });
    world.addComponent(enemy, game::Enemy{});

    engine::Logger::Info("Created player entity (ID: ", playerEntity_,
                         ") - Use WASD/Arrows to move!");
    engine::Logger::Info("Created enemy entity  (ID: ", enemy, ")");
}

void ClientApp::HandleRecorderHotkeys() {
    if (!window_) return;
    using State = engine::InputRecorder::State;

    if (glfwGetKey(window_, GLFW_KEY_F5) == GLFW_PRESS) {
        if (recorder_.GetState() == State::IDLE) {
            recorder_.StartRecording();
        }
    }
    if (glfwGetKey(window_, GLFW_KEY_F6) == GLFW_PRESS) {
        if (recorder_.GetState() == State::RECORDING) {
            recorder_.StopRecording("recording.bin");
        } else if (recorder_.GetState() == State::PLAYBACK) {
            recorder_.StopPlayback();
        }
    }
    if (glfwGetKey(window_, GLFW_KEY_F7) == GLFW_PRESS) {
        if (recorder_.GetState() == State::IDLE) {
            recorder_.StartPlayback("recording.bin");
        }
    }
}

void ClientApp::PublishPlayerInput() {
    if (playerEntity_ == engine::INVALID_ENTITY) return;
    auto& world = sim_.world();
    if (!world.hasComponent<game::PlayerInput>(playerEntity_)) return;

    auto& stored = world.getComponent<game::PlayerInput>(playerEntity_);

    if (recorder_.GetState() != engine::InputRecorder::State::PLAYBACK) {
        stored = keyboardPoller_.poll(window_);
    }
    recorder_.ProcessInput(stored);
}

void ClientApp::MainLoop() {
    lastFrameTime_ = static_cast<float>(glfwGetTime());

    while (window_ && !glfwWindowShouldClose(window_)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float frameTime   = currentTime - lastFrameTime_;
        lastFrameTime_    = currentTime;

        if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_, true);
        }

        HandleRecorderHotkeys();
        PublishPlayerInput();

        sim_.advance(frameTime);
        client::renderWorld(sim_.world(), renderer_, sim_.alpha());

        glfwSwapBuffers(window_);
        glfwPollEvents();

        frameCount_++;
        fpsTimer_ += frameTime;
        if (fpsTimer_ >= 1.0f) {
            std::string newTitle = title_ + " - "
                                 + std::to_string(frameCount_) + " FPS";
            glfwSetWindowTitle(window_, newTitle.c_str());
            frameCount_ = 0;
            fpsTimer_   = 0.0f;
        }
    }
}

void ClientApp::Run() {
    if (window_) MainLoop();
}

void ClientApp::Cleanup() {
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

} // namespace client
