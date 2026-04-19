# 0001 — Separating concerns: why `Engine` had to split

- **Date:** 2026-04-19
- **Commit:** *n/a (planning note; implementation lands as Phase 2.5 commits)*
- **Related:** [design/0001-simulation-split.md](../design/0001-simulation-split.md)
- **Principles:** Single Responsibility, Dependency Rule (Clean Architecture),
  Decoupling via data, Composition over inheritance

---

## What I built

*Planning note ahead of implementation.* The decision is: stop treating the
simulation, the window, the renderer, and the input recorder as one object
(`Engine`), and instead model them as separate objects connected by plain
data. The ECS `World` and the `PlayerInput` component become the seam.

## Why (the problem)

I want a headless server. The current `Engine` pulls GLFW and OpenGL into
every translation unit that touches the simulation, because everything is
nailed to one class. That's not a build problem — it's a *thinking* problem.
The code is telling me "you wrote this as a game, not an engine." A server
that can't run without a monitor is a tell.

A second smell: `Engine::Update` contains

```cpp
if (dynamic_cast<engine::RenderSystem*>(system.get()) == nullptr) {
    system->update(world, dt);
}
```

Any time code has to interrogate an object's type to decide whether to call a
method, the abstraction is wrong. The base class `System` is claiming
"everything that updates a world", but one of its subclasses (`RenderSystem`)
isn't that — it's a frame-rate-paced thing that reads world state and writes
pixels. Forcing it into `System` made `Engine` gain a hack to work around its
own type system.

## How I approached it

1. Asked: *what is the smallest thing the server needs?* Answer: the ECS
   world and the sim systems that mutate it, driven by a fixed tick.
2. Asked: *what does the client have that the server doesn't?* Answer: a
   window, a renderer, a keyboard, a recording feature, an FPS readout.
3. Drew a line between those two answers. Above the line:
   `engine::Simulation`. Below the line: `client::ClientApp`.
4. Found the only shared data: `PlayerInput` components on entities. That's
   the seam. `ClientApp` writes them from the keyboard; later, a network
   layer writes them from packets; the recorder reads/writes them too.
5. Killed the `System` membership of `RenderSystem` by turning it into a
   free function. If it doesn't fit in the abstraction, it doesn't belong.
6. Wrote [design/0001](../design/0001-simulation-split.md) before touching code.

## Principle(s) this demonstrates

- **Single Responsibility Principle.** `Engine` was doing roughly seven jobs.
  After the split, each class has one reason to change: `Simulation` changes
  when sim rules change; `ClientApp` when presentation changes; etc.
  Reference: Robert C. Martin, *Clean Code* ch. 10.

- **The Dependency Rule (Clean Architecture).** Inner layers (simulation)
  must not depend on outer layers (windowing, rendering, networking).
  Today, `InputSystem` depends *in the wrong direction* on GLFW. After the
  split, inputs arrive as data; the direction flips. Reference: Robert C.
  Martin, *Clean Architecture* ch. 17.

- **Decoupling via data, not interfaces.** I didn't introduce an
  `IInputProvider` interface. The seam is the `PlayerInput` component — a
  plain struct. Anything can write it. This is *more* flexible than a
  polymorphic interface, not less, and it costs zero virtual calls.

- **Composition over inheritance.** `RenderSystem : System` was pure
  inheritance smell: we inherited from `System` only to be stored in the
  same `vector`, then had to type-check at runtime. Replacing it with a free
  function deletes the inheritance and the `dynamic_cast` in one move.

- **YAGNI (in the rejected alternatives).** I considered a templated
  `Engine<Frontend>` and an `IFrontend` interface. Both are solving a
  problem I don't have (many frontends per process). The real problem is
  two *executables*, which is a link-time concern, not a runtime one.

## What I got wrong first

Initial instinct was to "just add a `#ifdef HEADLESS_SERVER` around the
GLFW bits." That would have worked technically and rotted the codebase —
every future system would have to remember which side of the `#ifdef` it
lived on. The smell that saved me was realising `InputSystem` itself would
need the `#ifdef`, which meant the *simulation* was encoding its frontend.
That's the Dependency Rule being violated. At that point the real fix
(separate objects, not separate branches of the preprocessor) was obvious.

## What I'd do differently

Should have done this split in Week 1, honestly. The cost was small then; it
grew every commit. Rule for next time: **any time a "main" class owns both a
simulation and a presentation layer, split it on day one, even if the
project is tiny.** The split is never cheaper than now.

## Takeaway for future me

- The type system is a mirror. If you're writing `dynamic_cast` to work
  around your own abstraction, the abstraction is wrong — don't paper over it.
- The best seam between a simulation and its frontend is usually a **plain
  data component**, not an interface.
- Write the design note before the code. It forces the alternatives section,
  which is where the real thinking happens.
