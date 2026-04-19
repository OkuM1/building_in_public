#pragma once
#include <GLFW/glfw3.h>

/**
 * @file Renderer.h
 * @brief OpenGL/GLFW platform renderer. Client-side only.
 * @see docs/design/0006-cmake-split.md
 */

namespace client {

/// @brief Thin immediate-mode wrapper over legacy OpenGL.
///
/// The API is intentionally minimal: clear the frame, draw a rectangle,
/// draw a circle. This class is a placeholder until the renderer grows
/// into something shader-based; keeping it small means swapping the
/// backend is a localised change.
class Renderer {
public:
    Renderer();
    ~Renderer();

    void Clear();
    void RenderRectangle(float x, float y, float width, float height,
                         float r, float g, float b);
    void RenderCircle(float x, float y, float radius,
                      float r, float g, float b);
};

} // namespace client
