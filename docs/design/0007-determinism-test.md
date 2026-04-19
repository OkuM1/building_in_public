# 0007 — Determinism test for `Simulation`

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 2.5 — Commit 6 of 6
- **Related:** [0003](0003-extract-simulation.md), [learning/0007](../learning/0007-test-the-invariant.md)

---

## Context

Every commit in Phase 2.5 has claimed "the game still plays the same" as
its verification. That is a human, visual check. If a future refactor
breaks determinism — the property that makes replay, rollback netcode,
lag compensation, and server-authoritative play possible — I want a
**machine** to notice.

The architecture of commits 0002–0005 makes this trivially testable:

- `engine::Simulation` has no platform dependencies.
- Inputs are a component on an entity (`game::PlayerInput`), not a
  callback into the keyboard.
- Ticks are deterministic by construction — each `step()` is a pure
  function of world state.

## Decision

> **Add `tests/test_simulation_determinism.cpp`. Build two identical
> `Simulation` instances, feed both the same canned input schedule,
> step both N times, assert that the resulting `Transform` on the
> player entity is bitwise identical in both.**

Schedule:

- 60 ticks: move right only.
- 60 ticks: move up and right (diagonal, tests normalisation).
- 60 ticks: no input (player coasts — but with no friction in the sim,
  this also tests that `Velocity` stays constant).
- 60 ticks: move left only.

At each phase boundary the expected position is computed from
`kPlayerSpeed` and `FIXED_DT` and asserted, so the test catches
both drift between two instances *and* a change in absolute behaviour.

A second test case runs 600 ticks with random-but-seeded inputs
(deterministic RNG) and asserts that two independent sim instances
produce the same final `Transform`.

## Alternatives considered

- **Unit-test individual systems** (InputSystem alone, MovementSystem
  alone). Useful, but the bug class we care about here is *composition*
  drift, which only shows when systems run together through
  `Simulation::step`. Start at the composition level; narrow if the
  test ever fails ambiguously.
- **Golden-file test** (dump Transforms to a file, compare to a
  committed baseline). Rejected for now: brittle under float formatting
  changes and encourages updating the golden without thinking. A
  closed-form expected position is more informative when it fails.

## Consequences

- Any future commit that breaks reproducibility across two independent
  `Simulation` instances will fail CI.
- Test runs in milliseconds; no window, no sleep, no I/O.
- Sets up the pattern for Phase 3/4 tests: "construct a sim in memory,
  step it, check components".

## Code pointers

- [`tests/test_simulation_determinism.cpp`](../../tests/test_simulation_determinism.cpp)
- [`CMakeLists.txt`](../../CMakeLists.txt) — test file added to `unit_tests`.

## Acceptance criteria

1. The new test compiles against `simulation` (no GLFW/GL).
2. `./build/unit_tests` passes all cases.
3. Commenting out any sim system in `Simulation::addSystem` causes the
   test to fail (verified manually).
