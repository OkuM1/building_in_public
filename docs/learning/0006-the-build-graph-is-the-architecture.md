# 0006 — The build graph is the architecture

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 5 of Phase 2.5)*
- **Related:** [design/0006](../design/0006-cmake-split.md), [learning/0003](0003-headless-first.md)
- **Principles:** Enforce invariants in the build, not in review; directory layout ≠ dependency graph

---

## What I built

Split the single `engine_core` static library into two: `simulation`
(platform-free, zero GL/GLFW) and `client_lib` (links `simulation` +
`glfw` + `OpenGL::GL`). Added a `server` executable that links only
`simulation`. Added a CMake `FATAL_ERROR` that aborts the configure
step if `simulation` ever picks up a GL link dep.

Verification: `nm libsimulation.a | grep gl` is empty. `ldd
./build/server` lists no GL libraries. `./build/client` and
`./build/unit_tests` still work.

## Why (the problem)

All the source-level discipline in the world doesn't prevent a future
developer (or future me at 2am) from adding `#include <GLFW/glfw3.h>`
to a file in `src/engine/`. The previous commits had an implicit rule
"engine files are platform-free"; the build happily compiled anything
that compiled, because `engine_core` linked GLFW anyway. The only
thing stopping the rot was human vigilance. That's a bad place to
hide an invariant.

## How I approached it

Two moves:

1. **Physically move** the GL-linking files (`Renderer.{h,cpp}`,
   `Rendering.{h,cpp}`) from `engine/` to `client/`. Namespaced them
   into `client::`. Now the directory and the dependency agree.
2. **Split the library** so the simulation target never sees GL. If
   someone adds `<GLFW/glfw3.h>` to a file under
   `src/engine/`, the compile will fail because CMake didn't give
   `simulation` the GLFW include path or link lib. The build graph
   enforces the rule mechanically.

I also added a CMake post-assertion (`if(_sim_link_libs MATCHES
"glfw|OpenGL") message(FATAL_ERROR ...)`) for defense in depth — if
someone adds `target_link_libraries(simulation PUBLIC glfw)` later,
configure aborts before a single file gets compiled.

## Principle(s) this demonstrates

- **Enforce invariants where they cost nothing to check.** Every
  `cmake ..` runs the check. No CI job, no reviewer, no convention
  to remember. A mechanical check you cannot bypass is infinitely
  cheaper than one that relies on vigilance.
- **Directory layout ≠ dependency graph (until you make them match).**
  `src/engine/platform/Renderer.cpp` *looks* like engine code because
  of its path; it is actually client code because of what it links.
  The only way to make "engine" a real label is to physically
  refuse to put client-shaped things there.
- **Symmetry between name, namespace, directory, target.**
  `client::platform::Renderer` lives in `include/client/platform/`,
  built into `client_lib`. A reader can derive any one of those four
  from any other. When they disagree, one of them is lying.

## What I got wrong first

- In [design/0004](../design/0004-render-as-free-function.md) I argued
  `Rendering` should live under `engine/systems/` because the
  *mechanism* (walk ECS, sort, draw) is engine-ish. That argument
  held up until I tried to split the CMake: `Rendering.cpp` includes
  `Renderer.h` which includes `<GLFW/glfw3.h>`, so the TU is
  unavoidably GL-linked. The original reasoning was right in theory
  and wrong in practice. Moved it to `client::render::` in this
  commit. Superseded that part of 0004.

## Takeaway for future me

- "This file is *conceptually* engine-ish" is not a good enough
  reason to put it in the engine target. The question is "does the
  translation unit pull in a banned header?". If yes, it's in the
  target that links that header's library — no matter how pure its
  logic feels.
- Every architectural rule you care about should be a build failure
  when violated. Reviewer-vigilance is not architecture; it is
  optimism.
- When a previous design note turns out to be wrong in a way that's
  revealed by a later change, write the correction into the new
  commit and note which part of the old note is superseded. Do not
  silently contradict yourself.
