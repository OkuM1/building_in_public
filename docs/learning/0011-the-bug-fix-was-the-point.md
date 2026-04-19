# 0011 — The bug fix that was the whole point

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 4 of Phase 3)*
- **Related:** [design/0011](../design/0011-delta-encoding.md)
- **Principles:** Asymmetric contracts, equality at the wire level, "write your way to the right API via tests"

---

## What I built

`encodeDelta<List>` / `applyDelta<List>`. Given two worlds and a
baseline tick, emit only the entities and components that differ.
Entities fully removed go in a separate removed-ids list. I also
added a third trait method, `Serializer<T>::equal`, that compares at
the wire level (quantised codes for `Transform`, raw for `Velocity`).

## Why (the problem)

Sending a full snapshot each tick is ~100 KB/s at 1k entities; the
budget is 32 KB/s. Delta encoding is how you claw that back. But the
interesting problem isn't *how to encode*, it's *what "changed" means*
when the only thing a reader has agreed to is the quantised
representation.

## How I approached it

Two design decisions mattered. Both of them I got wrong on the first
try and the tests caught it.

### Decision 1: What counts as "changed"?

First instinct: `a != b` on raw floats. That's wrong. After a
physics tick, floats drift sub-micron from rounding. Every `Transform`
on every entity would compare unequal, the delta would include every
entity every tick, and the whole optimisation would evaporate.

Correct: `equal` compares what actually goes on the wire. Two
`Transform`s that quantise to the same 16-bit position codes ARE
equal for delta purposes — the receiver couldn't tell them apart
anyway.

The lesson: **equality has to be defined at the level of the
contract being preserved**. For serialization the contract is "same
bytes on the wire." Equality at any finer grain is a lie.

### Decision 2: What counts as "present"?

First version of `maybeDiffOne` set the changed-mask bit whenever
presence OR value differed. That seems symmetric and tidy. It's
broken: if a component went from present-in-baseline to absent-in-
current, the bit is set — but then the encoder tries to write a
payload for a component that doesn't exist in current, and the
`getComponent` assert fires.

Fix: asymmetric rule. "Changed" only applies to components **present
in current**. Partial component removal (entity still exists, lost
one component) is a known gap in v1. Full entity removal goes in the
removed list.

This was the first test failure that revealed architectural intent:
the wire format is naturally asymmetric because the sender's job is
to describe what EXISTS NOW, with optional references to what used
to exist. The diff logic should reflect that.

## Principle(s) this demonstrates

- **Define equality at the level of the contract.** For netcode, the
  contract is bytes. For a UI cache, it's pixels. For a memoisation
  key, it's observable behaviour. Whenever you write `operator==`,
  ask: what is the receiver allowed to distinguish? If less than the
  raw representation, `operator==` should compare less too.
- **Asymmetric operations are often the truth.** Code gets shorter
  when you resist forcing symmetry onto things that aren't
  symmetric. The encoder writes NEW state; the decoder consumes it.
  An entity "going absent" is a different operation from "changing
  value" — don't jam them into one bit.
- **Tests catch intent bugs, not just mechanical bugs.** The "single
  Transform change produces single-component delta" test passed.
  The "removing an entity" test crashed. Both cases live-fire the
  same `maybeDiffOne` function; one caught a semantic error the
  type system couldn't. This is what tests are *for* — the
  mechanical ones could have been property-based, but the semantic
  ones want to be named examples.

## What I got wrong first

- Already covered in the two decisions above. Specifically:
  - Raw-float equality in `Transform::equal`. Test for sub-quantum
    drift caught it immediately. I hadn't written that test first;
    I wrote it as an afterthought and was surprised to see the
    delta was not empty. Better to have written the test first and
    noticed the gap during design.
  - Symmetric "changed = presence OR value differs" rule.
    `SIGABRT - Abort signal`. The crash is loud; silent corruption
    would have been worse. Grateful the assertion was in
    `ComponentArray::getData`.
  - Used `livingEntityCount--` without worrying about underflow. It
    never fired in tests but it's a lurking bug — noted for later.

## Takeaway for future me

- Whenever you add an equality comparison to a type that has a
  lossy serialization, *write the equality against the serialization*
  and use a ≈ comparison only for the raw floats when debugging.
  Wire-level equality is the one the protocol cares about.
- When a function returns void and has side-effects inside a fold
  expression, a bug in one branch can corrupt the output in ways
  that don't localise in the debugger. Prefer pure helpers that
  return their contribution and a fold that `|=`'s them all. That's
  why `maybeDiffOne` writes to a reference parameter `outMask`
  rather than calling the writer directly — debugging is easier
  when the effect is visible.
- Test failures are cheap information. Don't paper over the first
  crash — the bug under a crash is usually a design decision you
  haven't made yet.
