# 0010 — Traits over branches: a policy-free encoder

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 3 of Phase 3)*
- **Related:** [design/0010](../design/0010-snapshot-format.md)
- **Principles:** Compile-time dispatch, open/closed, separating mechanism from policy

---

## What I built

`engine::net::encodeSnapshot<List>(World&, tick, BitWriter&)` and its
inverse. `engine::net` knows how to walk a world and dispatch to
per-component serialisers; it knows nothing about any specific
component type. The game side writes `Serializer<Transform>`,
`Serializer<Velocity>`, `Serializer<PlayerInput>` specialisations in
its own header and declares a `ReplicationList<...>` type alias.
The engine and the game meet at the template boundary.

## Why (the problem)

The naive encoder is a function in `engine/` with an `if` block per
component type:

```cpp
if (world.has<Transform>(e)) { writePosition(...); writeAngle(...); }
if (world.has<Velocity>(e))  { out.writeFloat(v.vx); ... }
// ...
```

Every component the project ever adds has to be enumerated there.
The function grows linearly in the game's complexity and cannot live
in `engine/` without the engine importing `game/` headers. That
inversion is exactly backwards: the engine is the library, the game
is the caller.

Separately, "replicate this type the same way every time" is the kind
of rule a language can enforce. A missing serializer should fail at
compile time, not at runtime when the first snapshot goes out.

## How I approached it

Three pieces:

1. **A trait** (`Serializer<T>`) with a deliberately-undefined primary
   template. Any attempt to replicate an unspecialised type fails
   with a "is incomplete type" error at the point of use.
2. **A type list** (`ReplicationList<Components...>`) that gives the
   encoder a fixed, ordered set of types to consider. The position
   in the list is the bit position on the wire — that's a decision,
   documented and `static_assert`ed at size 8.
3. **Fold expressions** (`(expr, ...)`) in the engine header to
   unroll over the list. For each list entry, compile-time emit the
   mask-bit computation, the write call, the read call. No runtime
   dispatch, no vtable.

The whole engine-side file is ~150 lines and never names a component
type. The game-side file is ~80 lines and has one specialisation per
component. Adding a fourth component means adding one specialisation
and one entry to the list — zero edits to the engine-side code.

## Principle(s) this demonstrates

- **Mechanism vs policy.** The engine owns the mechanism: "given a
  list of types and their serialisers, walk the world and pack them."
  The game owns the policy: "these are the types that get replicated
  and here's how." When you can cleanly say what each side owns, the
  split belongs at a header boundary. When you can't, it doesn't.
- **Open/closed, via templates rather than inheritance.** The engine
  is closed for modification (it never has to change when components
  do) and open for extension (any new component is a new
  specialisation). Templates buy this without the runtime cost of
  virtual calls and without the code-organisation cost of base
  classes.
- **Compile-time errors over runtime crashes.** A typo'd component,
  a missing specialisation, a wrongly-sized mask — all of these are
  build failures. The alternative is a subtle netcode bug that shows
  up during playtest and takes two days to find.

## What I got wrong first

- First draft had `encodeSnapshot` in a `.cpp` file with an `extern
  template` declaration for the canonical `GameReplication`. That's
  the pattern for when you have one instantiation — but then you're
  paying all the machinery cost of templates and getting none of the
  flexibility. Moved the whole thing into the header. Templates that
  exist to be instantiated by callers belong in headers.
- My first `Serializer<Velocity>` used `writePosition` (quantised 16
  bits/axis). That's wrong — `kWorldExtent` is a *position* range;
  velocity can legitimately be ±10 units/sec, which clamps and
  destroys physics. Fixed to raw `float` with a note that it's a
  placeholder until Sumo Arena physics give us a max-velocity number
  to quantise against. Lesson: "the quantiser works on anything with
  a known range" is a shaped truth — and you only know the range once
  the thing it's quantising exists.
- The decoder originally asserted that every entity id referenced in
  the snapshot existed on the receiver. I relaxed this to a silent
  skip because "the snapshot references an entity I don't know
  about" is a Phase 5 spawn-event problem, not a Phase 3 format
  problem. This is the sort of decision worth flagging explicitly
  in the design note, so a reader in three months doesn't mistake
  the tolerance for a bug.

## Takeaway for future me

- When a system has an obvious "there will be N of these" axis,
  decide early whether N is fixed (use a tagged union / variant) or
  open (use a trait + list). Variants are cheaper but they're a wall;
  traits are costlier but they scale.
- A "mechanism" header should never import a "policy" header. If the
  direction of dependency feels wrong, the types probably want to
  flip: the caller provides the policy to the mechanism, not the
  other way around.
- Templates in headers are fine — don't contort the architecture to
  move them into a `.cpp`. The rule "headers for declarations, `.cpp`
  for definitions" is about separating compilation units, not about
  moral cleanliness.
