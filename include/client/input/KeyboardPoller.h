#pragma once

#include "game/components/GameComponents.h"

struct GLFWwindow;

/**
 * @file KeyboardPoller.h
 * @brief Reads the keyboard via GLFW and produces a `game::PlayerInput` value.
 *
 * This is the **only** place in the codebase that speaks `GLFW_KEY_*`. By
 * funnelling every keyboard read through here we keep the simulation
 * library (`engine::`) free of any windowing dependency, satisfying the
 * Dependency Rule from @ref docs/design/0001-simulation-split.md.
 *
 * A poller is stateless \u2014 it does not debounce, remember edges, or own a
 * window. Callers pass the window each frame and receive a fresh snapshot.
 *
 * @see docs/design/0002-decouple-inputsystem.md
 * @see game::PlayerInput
 */

namespace client {

/// @brief Stateless translator from keyboard state to `game::PlayerInput`.
///
/// Construct once on the client side, call @ref poll every frame before
/// advancing the simulation. The returned value is meant to be written
/// onto an entity's `PlayerInput` component (the simulation seam).
class KeyboardPoller {
public:
    /// @brief Sample the keyboard and produce a fresh input snapshot.
    ///
    /// @param window  The GLFW window whose keyboard state to read. Must
    ///                be the current context on this thread.
    /// @return        A `game::PlayerInput` describing which actions are
    ///                currently held. All fields default to `false` if
    ///                @p window is null.
    game::PlayerInput poll(GLFWwindow* window) const;
};

} // namespace client
