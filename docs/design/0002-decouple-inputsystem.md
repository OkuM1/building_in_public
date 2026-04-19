# 0002 — Decouple `InputSystem` from GLFW

- **Status:** Accepted (implemented in this commit)
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 1 of 6
- **Related:** [0001](0001-simulation-split.md), [learning/0002](../learning/0002-data-as-a-seam.md)

---

## Context

Today [`engine::InputSystem`](../../include/engine/systems/InputSystem.h)
takes a `GLFWwindow*` in its constructor, calls `glfwGetKey(...)` inside
`update()`, and owns an `engine::InputRecorder` that it drives from the same
method.

Consequences:

- The simulation library transitively depends on GLFW, so a headless server
  cannot exist (see [0001](0001-simulation-split.md)).
- The single `update(world, dt)` does **three** unrelated jobs: poll the
  keyboard, drive the recorder, translate input to velocity. Any future
  source of input (network, AI-bot, test harness) would have to either
  subclass `InputSystem` or be squeezed into `glfwGetKey`-shaped code.
- `InputSystem` cannot be unit-tested without a window.

We want the sim to treat input as *data that appears on entities*, and the
various producers of that data (keyboard today, network tomorrow, recorder
during playback) to be swappable without the sim knowing.

## Decision

> **Make the `game::PlayerInput` component the only input contract the
> simulation knows about. Move all GLFW polling and recorder plumbing out of
> `InputSystem` into client-side code.**

Concretely, in this commit:

1. Introduce a new namespace/directory `client/` with
   `include/client/input/KeyboardPoller.h` +
   `src/client/input/KeyboardPoller.cpp`. `KeyboardPoller` has one method:

   ```cpp
   game::PlayerInput poll(GLFWwindow* w) const;
   ```

   It is the *only* place in the codebase that mentions `GLFW_KEY_*`.

2. Rewrite `engine::InputSystem` so it:
   - has a default constructor (no `GLFWwindow*`),
   - no longer owns an `InputRecorder`,
   - reads the already-populated `game::PlayerInput` component on each
     entity and translates it to `game::Velocity`.
   
   `InputSystem`'s translation units no longer `#include <GLFW/glfw3.h>`.

3. Move the recorder and the F5 / F6 / F7 handling into `Engine` for the
   duration of Phase 2.5. Each frame `Engine` will:
   1. poll the keyboard via `KeyboardPoller` → write onto the player
      entity's `PlayerInput`,
   2. let the recorder overwrite `PlayerInput` in playback mode or capture
      it in record mode,
   3. run the simulation.
   
   Once `ClientApp` lands (commit 4), the recorder moves there. Moving it
   now too would balloon this commit.

The `engine_core` library still links GLFW because `Engine` and `Renderer`
do; that is fine for this commit. The GL/GLFW dependency will be expelled
from the `simulation` target in commits 2 and 5.

### Acceptance

- `include/engine/systems/InputSystem.h` contains no GLFW include.
- `src/engine/systems/InputSystem.cpp` contains no GLFW include, no
  reference to `InputRecorder`, and no `glfwGetKey` call.
- Game still plays identically: WASD / arrows move the green square;
  F5 / F6 / F7 still record / stop / play back.

## Alternatives considered

- **Introduce an `IInputSource` abstract class; pass it to `InputSystem`.**
  Rejected. A virtual call per frame per source, and it makes the sim
  vocabulary include "where input comes from." The sim does not care; only
  the value matters. A plain data component is a cheaper seam.

- **Leave the recorder inside `InputSystem`, just remove the GLFW call.**
  Rejected. The recorder operates at the `PlayerInput` layer and will later
  coexist with a network input source; both must live at the same level
  (the client). Keeping it in the sim would block that.

- **Do both `KeyboardPoller` *and* a `NetInputSource` stub now.**
  Rejected (YAGNI). No network yet; adding a stub is imaginary design.
  We will introduce it in Phase 4 when it carries weight.

## Consequences

**Positive**
- `InputSystem` becomes trivially unit-testable: construct a world, populate
  `PlayerInput`, step the system, assert `Velocity`.
- The `PlayerInput` seam is now real. Future producers (network, replay,
  AI bot) plug in at the same spot.
- One directory (`client/`) appears for the first time, foreshadowing the
  larger `ClientApp` split in commit 4.

**Negative / cost**
- `Engine` temporarily gains responsibilities (keyboard poll, recorder
  lifecycle). This is transitional; commit 4 moves them into `ClientApp`.
- Two files move. Tests that existed for `InputSystem` would need updating
  (none exist yet).

**Follow-ups**
- Commit 2: extract `engine::Simulation` out of `Engine`.
- Unit test for `InputSystem`: scripted `PlayerInput` → expected `Velocity`.
  Deferred to commit 6 where the determinism test fixture lands.

## Code pointers

- [include/engine/systems/InputSystem.h](../../include/engine/systems/InputSystem.h) (modified)
- [src/engine/systems/InputSystem.cpp](../../src/engine/systems/InputSystem.cpp) (modified)
- [include/client/input/KeyboardPoller.h](../../include/client/input/KeyboardPoller.h) (new)
- [src/client/input/KeyboardPoller.cpp](../../src/client/input/KeyboardPoller.cpp) (new)
- [include/engine/core/Engine.h](../../include/engine/core/Engine.h) (modified)
- [src/engine/core/Engine.cpp](../../src/engine/core/Engine.cpp) (modified)
- [CMakeLists.txt](../../CMakeLists.txt) (modified)

## References

- *Clean Architecture*, Martin — the Dependency Rule: `engine/` is an inner
  layer; it may not know about GLFW, which lives at the platform boundary.
- Mike Acton, CppCon 2014 "Data-Oriented Design and C++" — "the data is the
  problem", which in this codebase translates to "input is a component, not
  a method call."
