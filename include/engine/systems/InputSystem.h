#pragma once
#include "engine/ecs/System.h"
#include "game/components/GameComponents.h"
#include "engine/core/InputRecorder.h"
#include <GLFW/glfw3.h>

namespace engine {

class InputSystem : public System {
public:
    InputSystem(GLFWwindow* window) : window(window) {}
    
    void update(World& world, float dt) override;

private:
    GLFWwindow* window;
    InputRecorder recorder;
};

} // namespace engine
