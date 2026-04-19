# 0005 — Retiring the god-class

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 4 of Phase 2.5)*
- **Related:** [design/0005](../design/0005-introduce-clientapp.md), [learning/0001](0001-separating-concerns.md)
- **Principles:** SRP, namespaces as a design tool, delete-over-alias

---

## What I built

Deleted `Engine` entirely. Created `client::ClientApp` which owns the
window, the `Renderer`, the `KeyboardPoller`, the `InputRecorder`, the
player entity, and the real-time main loop. `main.cpp` is now five lines.

## Why (the problem)

`Engine` had been a transitional shell since commit 0003: it delegated
simulation to `engine::Simulation` and kept the client-shaped residue.
"Transitional" is a state that tries to stay permanent if you let it.
The file `engine/core/Engine.h` kept including `<GLFW/glfw3.h>`, so
every translation unit that touched `Engine` dragged GLFW along, even
ones that only wanted the simulation.

## How I approached it

Copied the methods into a new class in a new namespace and a new
directory, renamed where the namespace made things redundant (`sim_`
instead of the old free-standing `sim`, `window_` with trailing
underscore for consistency with the `client::` convention). Then
**deleted** `Engine.h` and `Engine.cpp`. No alias, no deprecation
path, no "keep the old name around until consumers migrate". There
are no external consumers; the only migration was `main.cpp`.

## Principle(s) this demonstrates

- **Namespaces as a design tool.** `engine::` now means "stuff a
  headless server can use". `client::` means "stuff that needs a
  window". The compiler enforces the invariant: if you write
  `#include "engine/..."` and it works, the code is headless-safe.
- **SRP.** `ClientApp`'s reason to change is "how does a windowed
  player-facing app drive a simulation". It no longer has to co-own
  "what does a simulation tick look like". When a replay viewer
  appears, it will be a second class in `client::`, not a second
  branch inside `ClientApp`.
- **Delete over alias.** Keeping old names around feels kind but it is
  a debt. For a pre-0.1 repo with no external consumers, deletion is
  strictly better than aliasing.

## What I got wrong first

- I initially left `Renderer` in `engine::platform/`. That was fine at
  the source level (the class didn't need a namespace change) but
  wrong at the build-graph level: a file under `engine/` linking GLFW
  means `engine_core` cannot be split. Commit 5 moves it to
  `client::platform`.

## Takeaway for future me

- When a class is labelled "transitional" in a comment, set a deadline
  or a deletion commit. Otherwise it becomes permanent.
- Directory layout and namespace match matter. If a file lives under
  `engine/` but does client-only things, the dependency graph will
  disagree with you, and the build graph is where the disagreement
  becomes expensive.
