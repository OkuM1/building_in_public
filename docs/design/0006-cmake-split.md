# 0006 — CMake split: `simulation` lib, `client` exe, `server` exe

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 5 of 6
- **Related:** [0001](0001-simulation-split.md), [0005](0005-introduce-clientapp.md), [learning/0006](../learning/0006-the-build-graph-is-the-architecture.md)

---

## Context

After commits 0002–0005 the **source code** is layered correctly:
`engine::` is platform-free, `client::` owns the window and renderer.
But the **build graph** still has one library, `engine_core`, that
links GLFW and OpenGL because `Rendering.cpp` and `Renderer.cpp` still
live under `src/engine/` and get compiled into it. A headless server
that linked `engine_core` would drag all of GL with it — so the
headless-server claim is not yet enforceable.

## Decision

> **Physically move the rendering files to `client/`, split the build
> into two libraries and two executables.**

Concretely:

1. Move
   - `include/engine/platform/Renderer.h` →
     [`include/client/platform/Renderer.h`](../../include/client/platform/Renderer.h),
     namespace `client::`.
   - `src/engine/platform/Renderer.cpp` →
     [`src/client/platform/Renderer.cpp`](../../src/client/platform/Renderer.cpp).
   - `include/engine/systems/Rendering.h` →
     [`include/client/render/Rendering.h`](../../include/client/render/Rendering.h),
     namespace `client::`.
   - `src/engine/systems/Rendering.cpp` →
     [`src/client/render/Rendering.cpp`](../../src/client/render/Rendering.cpp).

2. Split `CMakeLists.txt` targets:
   - **`simulation`** — static lib. Contains only platform-free sources:
     `Simulation`, `Logger`, `InputRecorder`, `World`, `InputSystem`,
     `MovementSystem`. **Does not link `glfw` or `OpenGL::GL`.**
   - **`client_lib`** — static lib. Contains `ClientApp`,
     `KeyboardPoller`, `Renderer`, `Rendering`. Links `simulation`,
     `glfw`, `OpenGL::GL`.
   - **`client`** — executable, `src/main.cpp`, links `client_lib`.
   - **`server`** — executable, `src/server/main_server.cpp`, links
     only `simulation`. Runs a bare tick loop, proving the build graph
     works without GL.
   - The backward-compat `main` target is removed. `client` is the
     name.

3. A CMake assertion enforces the separation:

   ```cmake
   get_target_property(_sim_libs simulation LINK_LIBRARIES)
   if(_sim_libs MATCHES "glfw|OpenGL")
       message(FATAL_ERROR "simulation must not link glfw/OpenGL")
   endif()
   ```

## Alternatives considered

- **Keep `Rendering` under `engine/systems/`** (as [0004](0004-render-as-free-function.md) originally argued).
  Rejected: that argument assumed rendering was pure world-walking
  logic. In practice `Rendering.cpp` includes `Renderer.h` which
  includes `<GLFW/glfw3.h>`, so it unavoidably pulls GL into whatever
  library compiles it. The cleanest fix is to put the whole rendering
  mechanism in `client/`.
- **A single `engine_core` with conditional compilation
  (`#ifdef HEADLESS_SERVER`).** Rejected: conditional compilation hides
  the dependency split from the compiler. The build graph should make
  the rule enforceable, not require developer discipline on every new
  file.

## Consequences

**Good**

- The build graph now enforces the architecture: the `simulation`
  target *cannot* compile against GL because CMake never passes it
  the include path / link libs. New engine code that accidentally
  `#include`s GLFW will fail to link.
- `./build/server` exists and runs. It ticks a simulation at 60 Hz and
  prints the tick counter. A proof-of-life for the headless story.
- `./build/client` is unchanged in behaviour from the user's point of
  view.

**Bad / deferred**

- `server` is a one-file toy today. A real server grows through Phase
  3 (serialization) and Phase 4 (UDP).
- [design note 0004](0004-render-as-free-function.md)'s "keep Rendering
  under engine/systems" is superseded by this commit. I'll mark it
  accordingly in its status field rather than rewrite.

## Code pointers

- [`CMakeLists.txt`](../../CMakeLists.txt) — new target split.
- [`src/server/main_server.cpp`](../../src/server/main_server.cpp) — headless main.
- [`include/client/platform/Renderer.h`](../../include/client/platform/Renderer.h),
  [`include/client/render/Rendering.h`](../../include/client/render/Rendering.h).

## Acceptance criteria

1. `simulation` target links neither `glfw` nor `OpenGL::GL`.
2. `./build/client` still plays the game identically.
3. `./build/server` compiles and runs; prints tick counter.
4. Unit tests green.
