# 0002 — Data as a seam

- **Date:** 2026-04-19
- **Commit:** *(this one)*
- **Related:** [design/0002](../design/0002-decouple-inputsystem.md)
- **Principles:** Dependency Rule, Single Responsibility, YAGNI, data-oriented design, composition over inheritance

---

## What I built

`engine::InputSystem` no longer knows what GLFW is. It now consumes a
`game::PlayerInput` component off each entity and writes a `game::Velocity`.
A new `client::KeyboardPoller` is the single place in the codebase that
mentions `GLFW_KEY_*`; it produces a `PlayerInput` value. `Engine` wires
the two together each frame — and, during Phase 2.5, also owns the
recorder that used to live inside `InputSystem`.

The simulation layer's translation units no longer `#include <GLFW/...>`.
That's the whole point: the same `InputSystem` can now run on a headless
server, fed by the network instead of a keyboard, without a single line
of it changing.

## Why (the problem)

Before today, `InputSystem::update` did three things: poll GLFW, drive the
`InputRecorder` state machine, and translate keys to a velocity. Every
future input producer — network, AI bot, replay harness — would have had
to impersonate a `GLFWwindow*` or subclass `InputSystem`. Worse, there
could be no headless server because the sim library transitively depended
on GLFW. [Design note 0001](../design/0001-simulation-split.md) had
already diagnosed this as the blocker for Phase 2.5; this commit is the
first concrete step that pays the diagnosis off.

## How I approached it

I had two candidate seams:

1. **Polymorphic seam** — give `InputSystem` an `IInputSource*` it calls
   each frame. `KeyboardSource`, `NetworkSource`, `ReplaySource`
   implement it.
2. **Data seam** — `InputSystem` reads the `PlayerInput` component that
   is already on the entity. Whoever wrote it doesn't matter.

I picked (2). Both decouple GLFW from the sim, but (1) adds an interface
and a virtual call for a shape I already have in the ECS. The component
**is** the interface. Whatever writes that component is an implementation
detail of the producer, not a contract the sim has to honour.

The hard part wasn't the split itself — it was moving the
`InputRecorder` out cleanly. It belongs on the client side (the server
will record inputs over the wire, not keystrokes), so `Engine` owns it
for now. A later commit will move it into `ClientApp`.

## Principle(s) this demonstrates

- **Dependency Rule (Clean Architecture).** Higher-level policy (the sim)
  must not depend on lower-level mechanism (GLFW). The only way to check
  this is to try to compile the sim without the mechanism — which is
  exactly what a headless server forces you to do.
- **Single Responsibility.** `InputSystem` now has one reason to change:
  "how do player-intent flags map to motion?". Polling changes when the
  window library changes; recording changes when the file format
  changes; neither touches `InputSystem` any more.
- **YAGNI.** I did *not* write `IInputSource`. The data-oriented seam
  already gives me the swap-ability I need. An interface with one real
  implementation is a trap.
- **Data-oriented / composition over inheritance.** The ECS already
  models "thing that has a `PlayerInput`". Re-using it as the input
  contract is cheaper than inventing a new polymorphism. Mike Acton's
  refrain — *"data is the interface"* — fits here.

## What I got wrong first

1. I initially left `InputRecorder` inside `InputSystem` thinking
   "recording is part of input." It isn't — recording is a **client-side
   policy** about what to do with the keystrokes before they become a
   component value. Moving it out was the moment the split felt right.
2. Two attempts at rewriting `InputSystem.h` via the editor left the
   file with *both* old and new class definitions concatenated, which
   the compiler happily reported as
   `error: redefinition of class engine::InputSystem`. Lesson: when a
   write-to-file tool misbehaves, `wc -l` before trusting, and fall back
   to a direct shell write.

## What I'd do differently

- Introduce a minimal determinism test *before* the refactor, not after.
  Then each commit in Phase 2.5 could prove it didn't regress behaviour
  instead of me eyeballing the game. This is already queued as Commit 6
  of the phase; it should have been Commit 0.
- Split the design note and the code change into two commits (note →
  code), so the note could be reviewed on its own. Good habit to build
  before the repo gets collaborators.

## Takeaway for future me

- When you're tempted to add an `ISomething*` interface, ask whether the
  ECS already has a component shape that encodes the same thing. If it
  does, the component **is** the seam.
- "Move the thing that knows about the platform" and "split
  responsibilities" are the same refactor viewed from two angles. Do
  them in one commit so the reasoning stays linked.
- The test of whether decoupling worked is not "does the code look
  cleaner". It is: *can I delete the platform library and still compile
  the sim?* If not, the seam is an illusion.
