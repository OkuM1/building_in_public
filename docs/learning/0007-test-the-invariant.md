# 0007 — Test the invariant, not the implementation

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 6 of Phase 2.5)*
- **Related:** [design/0007](../design/0007-determinism-test.md)
- **Principles:** Test behaviour not structure; determinism as a contract

---

## What I built

`tests/test_simulation_determinism.cpp` with three cases:

1. Two independently constructed `engine::Simulation`s, fed the same
   canned four-phase input schedule for 240 ticks, end up with
   bitwise-equal `Transform`s on the player entity.
2. A constant "move right" input for 60 ticks produces the
   closed-form expected position `FIXED_DT * 60 * kPlayerSpeed`
   (catches changes to the absolute behaviour, not just drift
   between instances).
3. A seeded-random 600-tick input stream is reproducible when run
   twice with the same seed.

All three run in the `simulation` target, no GLFW, no window, no
sleep. Total runtime is milliseconds.

## Why (the problem)

Every Phase 2.5 commit's verification up to this point was "I played
the game and it felt the same". That works once. It does not work the
second time someone refactors `Simulation`. Deterministic simulation
is the contract that Phase 3 (snapshots) and Phase 4 (UDP/rollback)
are going to depend on — and if it breaks silently during a refactor,
every one of those later commits gets poisoned.

## How I approached it

I almost wrote three unit tests for individual systems
(`InputSystem` alone, `MovementSystem` alone). That would have tested
the *implementation* of each system. But the bug we actually care
about is **composition drift**: two systems individually pure,
composed through `Simulation::step`, drift apart across two
instances. Testing at the `Simulation` level is the right layer
because that's the contract the rest of the codebase consumes.

I added the closed-form expected-position check as a second layer.
The identity-across-instances check catches "this refactor broke
determinism". The closed-form check catches "this refactor *also*
changed the sim speed / clamping / normalisation, you just didn't
notice because both instances agreed on the new wrong answer".

## Principle(s) this demonstrates

- **Test the contract, not the innards.** The contract is: same
  inputs → same state. Not: `MovementSystem::update` touches `x`
  before `y`. Tests against innards break when you refactor the
  innards; tests against contracts break only when the contract
  breaks, which is exactly when you want to know.
- **Invariants deserve machine enforcement.** Determinism is the
  kind of claim that rots silently. A failing test is a siren; a
  human check is a shrug.
- **Fast tests or no tests.** These tests run in a few ms because
  `Simulation` has no platform dependencies. That's not an accident
  — it's the payoff of the whole Phase 2.5 refactor. Testability was
  the lead metric.

## What I got wrong first

- First draft of the test tried to import `ClientApp` to "test the
  whole stack end to end". I caught myself: `ClientApp` needs a
  window. Linking the test against `client_lib` would have re-
  introduced GLFW into the test binary, defeating the whole point.
  Tests link `simulation` only; that's what "testable" means in
  practice.

## Takeaway for future me

- For every contract the system advertises (determinism, idempotence,
  termination, bounded latency), add a test that would fail if it
  broke — even if the contract "obviously holds" today. Obvious is
  how contracts rot.
- Two kinds of assertion: one that two runs agree, one that they
  agree on the *right* answer. Need both.
- When writing a test feels like it'd pull in a heavy dependency,
  the right fix is almost always to re-layer the code so the test
  is cheap — not to skip the test.
