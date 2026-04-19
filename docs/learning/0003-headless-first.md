# 0003 — Headless-first: making the simulation a thing that exists without a window

- **Date:** 2026-04-19
- **Commit:** *(this one)*
- **Related:** [design/0003](../design/0003-extract-simulation.md), [learning/0001](0001-separating-concerns.md)
- **Principles:** SRP, Dependency Rule, separation of clocks, YAGNI

---

## What I built

There is now a class `engine::Simulation` that owns the ECS world, the
list of simulation systems, the fixed-step accumulator, and a monotonic
`tick` counter. It has no idea a window or a network exists. `Engine` —
which used to own all of that — now delegates to it and keeps only the
client-shaped things (window, renderer, keyboard, recorder). The
`dynamic_cast<RenderSystem*>` that used to sort systems into "render vs
not render" every frame is gone: render lives outside the systems
vector, as a direct member.

I verified the headless invariant empirically: `g++ -H` on
`Simulation.cpp` prints no GLFW or GL headers in its transitive include
tree. That's the whole point of the commit — *the sim library can
compile on a box without those libraries installed.*

## Why (the problem)

Before today, `engine_core` transitively linked GLFW and OpenGL from
every translation unit because `Engine.h` dragged `<GLFW/glfw3.h>` into
everything, and the `World` + systems lived on `Engine`. A headless
server executable was a polite fiction: I could pretend it existed in
CMake, but the link would fail without GL.

There was also a design smell staring back at me every time I read
`Engine::Update`:

```cpp
for (auto& system : systems) {
    if (dynamic_cast<engine::RenderSystem*>(system.get()) == nullptr) {
        system->update(world, dt);
    }
}
```

That `if-not-render` filter is a type system workaround for putting two
kinds of things (sim systems and render systems) in one list. The
correct response is not a cleverer filter, it's to stop putting them in
the same list.

## How I approached it

I resisted two temptations:

1. **Templating `Simulation` on a `Clock`.** Tempting, because "advance
   time" is where clock choices live. But I only have one clock
   (`glfwGetTime`) and one use-case (real-time play). A headless server
   will just call `step()` in a loop. No template parameter needed.
2. **Leaving `RenderSystem` inside the systems vector.** I could have
   moved the whole vector into `Simulation` and kept the dynamic_cast
   filter. That would have buried the smell instead of fixing it.

Instead: `Simulation::addSystem` for sim systems; `RenderSystem` is a
direct `std::unique_ptr<RenderSystem>` on `Engine`. The render loop now
reads like English: `render_->update(sim.world(), sim.alpha())`.

The trickiest piece was deciding where the interpolation alpha lives.
It's computed from the simulation's accumulator (`accumulator / FIXED_DT`),
so `Simulation::alpha()` returns it. But alpha is only *meaningful* to
the renderer. That's fine — `Simulation` exposes it as a read-only
number; only the client consumes it. The server, which doesn't render,
will simply ignore it.

## Principle(s) this demonstrates

- **Single Responsibility.** `Simulation` now has one reason to change:
  "how does the world tick". `Engine` has one reason to change: "how
  does a player-facing window get fed from a simulation". Recorder
  plumbing, FPS counting, and window poking all touch `Engine` but
  none touch `Simulation`.
- **Dependency Rule.** High-level policy (the world's deterministic
  step function) no longer depends on low-level mechanism (GLFW).
  Verified mechanically, not by inspection — `g++ -H` is the lint.
- **Separation of clocks.** Real time (GLFW), simulation time
  (accumulator → fixed steps), and render time (interpolation alpha)
  are now three different numbers that live in the layer that owns
  them. This is what lets the server use a different real-time loop
  (e.g. `std::this_thread::sleep_until`) without touching `Simulation`.
- **YAGNI.** No `IClock`, no `ISimulationBackend`, no template
  parameter. When a second clock appears, I'll add it. Today there's
  one, so the code shows one.

## What I got wrong first

- **Docstring typo.** The initial `Simulation.h` file-comment contained
  the literal substring `/*` (trying to say "`<GLFW/*>`"), which GCC
  dutifully warned about inside a `/* ... */` block. Every
  documentation-heavy header should be compiled with `-Wall` before you
  trust it.
- **Accumulator clamp placement.** I first kept the 0.25 s clamp in
  `Engine::MainLoop`. That meant the clamp belongs to wall time, which
  is correct *but* a server running 60 Hz ticks against
  `steady_clock::now()` wants the same behaviour. Moving the clamp
  into `Simulation::advance` makes it a property of the simulation
  loop, not of one particular client.

## What I'd do differently

- Write the determinism test (Commit 6) *first*, as a guard rail. Right
  now I am proving correctness by playing the game. Next time a
  refactor like this should be fenced off with a test that steps the
  sim N times with canned inputs and diffs the resulting `Transform`s.
- Move the design note ADR update ("Status: Implemented") into its own
  follow-up commit once the whole phase lands, so each commit's diff is
  purely code. A small thing but it keeps the diff visually on-topic.

## Takeaway for future me

- The sign that a decoupling is real is a **mechanical test**: compile
  the lower layer, `g++ -H`, and assert no banned header appears. If
  you can't state that check, the decoupling is wishful thinking.
- Putting two kinds of things in one container and then filtering is
  almost always the wrong answer. The right answer is two containers.
  The filter code you don't write is more valuable than the filter
  code you do.
- Own the clocks. A simulation that owns its fixed-step accumulator and
  its tick counter is a simulation you can hand to a test, a replay, or
  a server. A simulation whose clock lives in someone else's main loop
  is stuck in that main loop forever.
