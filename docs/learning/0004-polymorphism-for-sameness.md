# 0004 — Polymorphism should represent sameness, not history

- **Date:** 2026-04-19
- **Commit:** *(this one)*
- **Related:** [design/0004](../design/0004-render-as-free-function.md), [learning/0003](0003-headless-first.md)
- **Principles:** Prefer free functions to classes with one method, polymorphism for substitutability (LSP), YAGNI

---

## What I built

Deleted `engine::RenderSystem`. Replaced it with a single free function
`engine::renderWorld(World&, Renderer&, float alpha)` declared in
`engine/systems/Rendering.h`. `Engine` no longer owns a
`unique_ptr<RenderSystem>`; it just calls the function once per real
frame.

## Why (the problem)

`RenderSystem` inherited from `engine::System` — an abstract base with
one virtual: `update(World&, float dt)`. After [commit 0003](../design/0003-extract-simulation.md),
simulation systems live inside `engine::Simulation`, and `RenderSystem`
lived outside it on `Engine`. So the inheritance was load-bearing for
nothing. Worse, it made the `dt` parameter mean two entirely different
things (fixed timestep vs interpolation alpha) depending on which
subclass you were looking at. Liskov would not approve.

Four downsides to keeping the class:

1. **Lying type signature.** `update(World&, float)` looked identical
   to a sim system, but the float meant something else entirely.
2. **Invitation to regress.** "It's a `System`, put it in the systems
   vector." Exactly the configuration I spent [commit 0003](../design/0003-extract-simulation.md)
   pulling apart.
3. **Ownership overhead.** `std::unique_ptr<RenderSystem>`, null check,
   constructor forwarding a `Renderer&` reference. All of it to reach
   one virtual call.
4. **Awkward handoff to `ClientApp`.** Commit 4 wants to move rendering
   behind the window-owning client. A free function is trivial to
   call from a new owner; a polymorphic object is a lifetime puzzle
   for no gain.

## How I approached it

Straight replacement. The body of `RenderSystem::update` becomes the
body of `renderWorld`. The member `Renderer& renderer` becomes a
parameter. The `alpha` parameter was already called `alpha` inside
the function, so I just renamed the signature to match reality.

The only real decision was *where* the file lives. `Rendering.h` could
have gone under `client/render/` since only clients render. I left it
under `engine/systems/` because the ECS walking and interpolation logic
are engine concerns — the **mechanism**. The **driver** (who owns the
window, when to call it, what to swap afterward) is the client. Commit
4 (`ClientApp`) will be the driver. This keeps the split along "what
it does" rather than "who ends up calling it".

## Principle(s) this demonstrates

- **Prefer free functions to classes with one method.** A class whose
  entire API surface is one `update()` is a function with a more
  expensive call site. Free functions compose better, test easier, and
  can be passed as `std::function` / function pointer / template
  argument if you ever need to swap the renderer.
- **Polymorphism represents substitutability, not history.** Two types
  share a base if you would ever want to call them interchangeably at
  the same call site with the same meaning. `InputSystem` and
  `MovementSystem` satisfy that (both run in the step loop, both
  receive `FIXED_DT`). `RenderSystem` never did — its "dt" wasn't a
  dt. Sharing the base was a coincidence of history, not a shared
  contract. Sean Parent's phrasing applies: *inheritance is the base
  class of evil*; use it only when the polymorphism is actually
  needed.
- **YAGNI.** I didn't introduce an `IRenderer` interface, a
  `RenderParams` struct, or a camera. Today the function has three
  parameters; when it needs a camera I'll add one. Speculative shape
  is worse than late shape.

## What I got wrong first

- **Almost put the header under `client/render/`.** Would have been
  wrong: `client` shouldn't have to know about `game::Transform` or
  interpolation math. Those are engine concepts. The one thing that's
  client-shaped is *when* you call the function, not *what* it does.
  Caught myself by asking "would a server ever want this?" — yes, a
  server might emit a debug image of its world-state for regression
  tests. So the function stays in `engine/`, and only its *caller*
  lives in `client/`.

## What I'd do differently

- Rename `alpha` in `System::update(World&, float dt)` is a parallel
  cleanup I noticed in passing — `dt` is a lie for every system other
  than a pure sim one, but now that `System` *only* means sim system,
  `dt` is correct again by subtraction. I almost refactored the name
  mid-commit; I stopped because it wasn't the goal of this commit.
  Hold onto scope.

## Takeaway for future me

- When you catch yourself writing `class Foo` with only one public
  method and a captured reference, stop and write the function
  instead. If you later need state, you can always promote it back to
  a class; the reverse is harder.
- Inheritance is a claim that two types are substitutable. If the
  substitution would be a lie (the base's contract doesn't hold for
  the derived class), inheritance is wrong no matter how convenient
  the shared base felt.
- "What it *does*" and "who *calls* it" are two different questions
  and want two different locations in the repo. Split along what it
  does; the caller's home is incidental.
