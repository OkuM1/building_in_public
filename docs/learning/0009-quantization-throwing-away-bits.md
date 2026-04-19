# 0009 — Quantisation: throwing away bits you never needed

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 2 of Phase 3)*
- **Related:** [design/0009](../design/0009-quantization.md)
- **Principles:** Pay only for the bits you use; centralise wire constants; pure functions compose

---

## What I built

A tiny module, `engine::net::Quantize`, that converts a float in a
known range to a fixed-width integer code and back. Plus two typed
wrappers: `packPositionComponent` (16-bit, `[-16, +16]`) and
`packAngle` (8-bit, `[0, 2π)`). Plus stream shortcuts that plug the
packer into `BitWriter` / `BitReader` from Commit 1.

All of it is free functions. No classes, no state, no configuration
surface beyond three `constexpr` constants at the top of the header.

## Why (the problem)

A `float` is 32 bits of "general-purpose number": exponent, mantissa,
sign, enough range to span subatomic to cosmic scales. A Sumo Arena
x-coordinate is a number in `[-16, +16]` with sub-millimetre
resolution. Paying 32 bits for that is paying for 28 bits of *range*
I don't have and *precision* I can't see. At 1000 entities × 20
snapshots/sec × three floats per `Transform`, that waste is the
majority of the bandwidth budget.

Quantisation is the fix: pick a range, pick a bit width, do the
affine map, forget the exponent ever existed.

## How I approached it

The only interesting design question was *where the constants live*.
Three options:

1. Caller chooses. Every `writePosition` call specifies
   `kWorldExtent` and `kPositionBits`. Flexible, and also a recipe
   for a wire format that differs at every call site.
2. Per-component constants baked into per-component serializers
   (Commit 3). Encapsulated, but splits the knowledge: to read a
   snapshot you have to know which component owns which constant.
3. One central block of `constexpr` at the top of `Quantize.h`.
   Changing `kPositionBits` is a recompile-once wire format bump.
   The callers write `kPositionBits`, not a magic number.

I picked (3). The reason is the same reason you centralise HTTP
status codes or physics constants: these numbers appear in the
protocol contract and the protocol contract is a single thing. A
diff that changes `kPositionBits` is a wire-breaking change; you
want it to look like one.

The implementation itself is two lines of arithmetic. The decision
wasn't "how to quantise" — it was "where the number that makes it a
wire format lives."

## Principle(s) this demonstrates

- **Pay only for the bits you use.** Bandwidth is a budget like
  memory or CPU. A 32-bit float on the wire isn't "free" just
  because it's small on disk — at 60 Hz × 1000 entities × 3 fields,
  you're burning a KB every frame you didn't have to.
- **Centralise the contract surface.** The wire format is defined
  by `kWorldExtent`, `kPositionBits`, `kAngleBits`. Three
  `constexpr` values, one header. A new engineer reading the
  snapshot encoder doesn't have to chase numbers.
- **Pure functions compose.** `packFloatInRange` knows nothing about
  `BitWriter` or `Position`. `packPositionComponent` is three lines
  that call it. `writePosition` is two lines that call *that*. Each
  layer is independently testable and the total complexity is
  roughly the sum of the layers, not the product.

## What I got wrong first

- First draft of `packFloatInRange` used `static_cast<uint32_t>` on a
  pre-scaled float without the `+ 0.5f`. That's truncation, not
  rounding, which biases the entire quantised range downward by half
  a step. The test for "0.0 in `[-1, 1]` with 8 bits should map to
  128" caught it: truncation produced 127. Reminder that tests at
  endpoints are necessary but not sufficient — you need one at the
  midpoint too, precisely because the bug is most visible away from
  the edges.
- I originally wrote `packAngle` without the modulo-2π reduction,
  figuring callers would pass normalised angles. Wrote the test "what
  if a caller passes `0.5 + 2π`?" and the answer was "it clamps to
  `2π` and loses precision". Added the `fmod` + branch. A callee
  that assumes pre-normalised input is a bug-magnet.

## Takeaway for future me

- When a number appears in more than one file and represents an
  interoperability contract, put it in one file with a name. Then
  the diff that changes the contract looks like a contract change.
- Endpoints and midpoint. Always both. Quantisation rounding bugs
  hide at the interior and only show up when you test it.
- When a pure function needs to be robust to "user error in the
  input", do the robustness in the function, not in a comment
  telling callers what they must do. Callers don't read comments
  — they read compiler errors.
