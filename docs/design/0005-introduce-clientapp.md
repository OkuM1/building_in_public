# 0005 — Introduce `client::ClientApp`, retire `Engine`

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 4 of 6
- **Related:** [0001](0001-simulation-split.md), [0003](0003-extract-simulation.md), [0004](0004-render-as-free-function.md), [learning/0005](../learning/0005-retiring-the-god-class.md)

---

## Context

After commits 0002–0004, `Engine` is a transitional shell that still owns:

- the GLFW window + OpenGL context,
- the `Renderer`,
- a `client::KeyboardPoller`,
- an `engine::InputRecorder` + F5/F6/F7 hotkey logic,
- a fixed-rate FPS counter + window-title writer,
- the real-time main loop,
- and delegates simulation to `engine::Simulation`.

All of those except "delegate sim" are **client-side concerns**. Keeping
them in `engine/core/Engine.{h,cpp}` is a historical artifact — the file
is in the engine tree but the code is a client. Leaving it there
contaminates the dependency graph: `engine_core` still has to link GLFW
because `Engine` is inside it.

## Decision

> **Move all of `Engine`'s responsibilities into a new
> `client::ClientApp`. Delete `Engine` entirely. `src/main.cpp` becomes
> a thin driver that constructs a `client::ClientApp` and calls `Run()`.**

Concretely, this commit:

1. Creates [`include/client/ClientApp.h`](../../include/client/ClientApp.h)
   + [`src/client/ClientApp.cpp`](../../src/client/ClientApp.cpp) in the
   `client::` namespace. It owns the window, renderer, simulation,
   keyboard poller, input recorder, player entity, and the real-time
   loop.

2. Deletes `include/engine/core/Engine.h` and
   `src/engine/core/Engine.cpp`. `engine_core` translation units no
   longer transitively include `<GLFW/glfw3.h>` through an `engine/`
   header.

3. Rewrites `src/main.cpp`:

   ```cpp
   int main() {
       client::ClientApp app(640, 480, "Dungeon Crawler - ECS Demo");
       app.Run();
   }
   ```

4. Updates `CMakeLists.txt` to swap `src/engine/core/Engine.cpp` for
   `src/client/ClientApp.cpp`.

The deeper CMake split into a GL-free `simulation` static library and
GL-linked `client`/`server` executables is Commit 5. This commit is the
source-tree precondition; CMake changes here are the minimum to keep
the build green.

## Alternatives considered

- **Keep `Engine` as a thin alias / wrapper around `ClientApp`.** Rejected:
  aliasing pays the cost of an extra name for no benefit. Nobody
  depends on `Engine` externally (it's a pre-0.1 project). Delete it.
- **Split `ClientApp` further (`Window`, `FrameClock`, `RecorderUI`).**
  YAGNI. One class of ~200 lines with focused methods is easier to
  read and refactor than three half-populated ones. Split when a
  second caller wants just one of them.

## Consequences

**Good**

- `engine_core` no longer references GLFW in any `engine/` header.
  After Commit 5, we can split it into a `simulation` static library
  that does not link GL/GLFW at all.
- The namespace story is now clean: `engine::` is platform-free sim,
  `game::` is components/gameplay, `client::` owns window + rendering
  driver + input polling. `main.cpp` is five lines.
- `ClientApp` is the single place that knows "a frame looks like: poll
  hotkeys → publish input → advance sim → render → swap → events". If
  that recipe changes, it changes in one place.

**Bad / deferred**

- `ClientApp` is still one class. It will split when a second client
  mode exists (e.g. a replay viewer that reuses the renderer but has
  no keyboard).
- The existing `main` CMake target is now redundant with `client` (they
  both build `src/main.cpp`). Commit 5 removes `main`.

## Code pointers

- [`include/client/ClientApp.h`](../../include/client/ClientApp.h)
- [`src/client/ClientApp.cpp`](../../src/client/ClientApp.cpp)
- [`src/main.cpp`](../../src/main.cpp)
- [`CMakeLists.txt`](../../CMakeLists.txt)

## Acceptance criteria

1. `Engine` class and its files no longer exist.
2. `grep -r GLFW include/engine src/engine` returns nothing.
3. Build + unit tests green. `./build/client` runs and plays
   identically (WASD, F5/F6/F7).
