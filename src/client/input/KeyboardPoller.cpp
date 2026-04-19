#include "client/input/KeyboardPoller.h"

#include <GLFW/glfw3.h>

/**
 * @file KeyboardPoller.cpp
 * @brief Implementation of @ref client::KeyboardPoller.
 * @see docs/design/0002-decouple-inputsystem.md
 */

namespace client {

game::PlayerInput KeyboardPoller::poll(GLFWwindow* window) const {
    game::PlayerInput input{};
    if (!window) {
        return input;
    }

    const auto held = [window](int key) {
        return glfwGetKey(window, key) == GLFW_PRESS;
    };

    input.moveUp    = held(GLFW_KEY_W) || held(GLFW_KEY_UP);
    input.moveDown  = held(GLFW_KEY_S) || held(GLFW_KEY_DOWN);
    input.moveLeft  = held(GLFW_KEY_A) || held(GLFW_KEY_LEFT);
    input.moveRight = held(GLFW_KEY_D) || held(GLFW_KEY_RIGHT);
    input.attack    = held(GLFW_KEY_SPACE);
    input.dodge     = held(GLFW_KEY_LEFT_SHIFT);

    return input;
}

} // namespace client
