#include "engine/systems/InputSystem.h"
#include "engine/ecs/Entity.h"
#include <cmath>

namespace engine {

void InputSystem::update(World& world, float dt) {
    // Handle Recording Controls
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
        if (recorder.GetState() == InputRecorder::State::IDLE) {
            recorder.StartRecording();
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS) {
        if (recorder.GetState() == InputRecorder::State::RECORDING) {
            recorder.StopRecording("recording.bin");
        } else if (recorder.GetState() == InputRecorder::State::PLAYBACK) {
            recorder.StopPlayback();
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS) {
        if (recorder.GetState() == InputRecorder::State::IDLE) {
            recorder.StartPlayback("recording.bin");
        }
    }

    // Process input for all entities with PlayerInput component
    for (EntityId entity = 0; entity < MAX_ENTITIES; ++entity) {
        if (!world.hasComponent<game::PlayerInput>(entity)) {
            continue;
        }
        
        auto& input = world.getComponent<game::PlayerInput>(entity);
        
        // Only read keyboard if NOT playing back
        if (recorder.GetState() != InputRecorder::State::PLAYBACK) {
            // Read keyboard state
            input.moveUp = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
            input.moveDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
            input.moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
            input.moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
            input.attack = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            input.dodge = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        }

        // Process via recorder (saves if recording, overwrites if playing back)
        recorder.ProcessInput(input);
        
        // Update velocity based on input (if entity has velocity)
        if (world.hasComponent<game::Velocity>(entity)) {
            auto& velocity = world.getComponent<game::Velocity>(entity);
            
            float speed = 0.5f; // Units per second
            velocity.vx = 0.0f;
            velocity.vy = 0.0f;
            
            if (input.moveUp) velocity.vy += speed;
            if (input.moveDown) velocity.vy -= speed;
            if (input.moveLeft) velocity.vx -= speed;
            if (input.moveRight) velocity.vx += speed;
            
            // Normalize diagonal movement
            if (velocity.vx != 0.0f && velocity.vy != 0.0f) {
                float length = sqrtf(velocity.vx * velocity.vx + velocity.vy * velocity.vy);
                velocity.vx = (velocity.vx / length) * speed;
                velocity.vy = (velocity.vy / length) * speed;
            }
        }
    }
}

} // namespace engine
