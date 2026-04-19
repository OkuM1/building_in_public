# 0009 — Quantised position & angle on the wire

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 3 (Serialization & snapshots) — Commit 2/5
- **Related:** [0008](0008-bitstream.md) — depends on `BitStream`

## Context

A snapshot budget of < 4 KB for 1k entities means ~4 bytes/entity for
everything. A raw `Transform` (`float x, y, rotation`) is 12 bytes —
three times the budget on position alone. The float's mantissa and
exponent are paying for range and precision the game doesn't need:
the arena is bounded, positions never exceed ±16 units, and ~0.5 mm
resolution is indistinguishable from exact.

Uniform quantisation fixes that. A 16-bit code over `[-16, +16]` gives
~2.4e-4 max error per axis — below screen-pixel scale — and costs a
third of the bits.

## Decision

Three public surfaces in `engine::net` (new TU `Quantize.{h,cpp}`):

1. **Generic helpers.** `packFloatInRange(v, min, max, bits)` and its
   inverse. Pure, stateless; the building block everything else is
   defined in terms of.
2. **Typed constants + helpers.** `kWorldExtent = 16.0f`,
   `kPositionBits = 16`, `kAngleBits = 8`, plus `packPositionComponent`
   / `unpackPositionComponent` / `packAngle` / `unpackAngle`. These
   pin the *wire format* — changing `kPositionBits` is a wire
   break; that's intentional and visible.
3. **Stream shortcuts.** `writePosition(BitWriter&, x, y)` and
   `writeAngle(BitWriter&, radians)` plus symmetric reads. These are
   what the snapshot encoder in Commit 3 calls; the caller never has
   to think about bit widths.

All of it lives in the `simulation` target — zero platform deps. The
CMake GL-free invariant still holds.

### Numbers

| Field | Range | Bits | Step | Max error |
|---|---|---|---|---|
| Position (per axis) | `[-16, +16]` | 16 | 32 / 65535 | 2.44e-4 |
| Angle               | `[0, 2π)`    | 8  | 2π / 255   | 0.0246 rad ≈ 1.41° |

A full `Transform` on the wire now costs `16 + 16 + 8 = 40 bits = 5
bytes`, down from 12. `PlayerInput` (6 bools) costs 6 bits = 1 byte
with room. That's the budget envelope Commit 3's snapshot encoder
will operate within.

## Alternatives considered

1. **Non-uniform (logarithmic) quantisation** for position, to give
   more precision near zero. Tempting for physics-heavy netcode but
   adds an exp/log pair per axis per entity per snapshot, and the
   arena is small enough that uniform error is already invisible.
   Rejected for complexity cost.
2. **Per-component bit width** set at call sites rather than in a
   central constant. More flexible but turns the wire format into a
   thousand ad-hoc decisions. Centralising the constants in
   `Quantize.h` means one recompile shifts the whole wire. Accepted.
3. **Store the pre-quantised `uint32_t` in a `PackedTransform`
   component** so the client reads packed data directly. Premature —
   the client always wants a `float` for rendering. Revisit if
   profiling shows decode overhead dominating.
4. **Half-floats (IEEE 754 binary16)** for position. Only 16 bits,
   cheap, but non-uniform error across the range and still paying for
   exponent bits we don't use. Rejected.

## Consequences

- **Enables** Commit 3's per-component `serialize(BitStream&, T&)`
  for `Transform`. Without this commit the snapshot encoder would
  have burned 3× the budget on position alone.
- **Locks** the wire format to `kWorldExtent = 16.0f`. Any future
  map larger than that is a format break — which is fine pre-1.0,
  but once the protocol has a version number we'll need either a
  bigger `kWorldExtent`, a per-snapshot scale, or a per-arena
  override. Leave the note; don't solve it now.
- **Accepts** the uniform-error tradeoff. If a Sumo Arena mode ever
  wants sub-mm precision for a push-meter, we'll revisit.

## Code pointers

- [include/engine/net/Quantize.h](../../include/engine/net/Quantize.h)
- [src/engine/net/Quantize.cpp](../../src/engine/net/Quantize.cpp)
- [tests/test_quantize.cpp](../../tests/test_quantize.cpp) — 8 cases,
  32 assertions; covers endpoints, clamping, error bounds, modulo-2π
  reduction, and round-trips through `BitStream`.

## Open questions

- **Wire format versioning.** Not needed yet; the client and server
  build from the same commit. Will need a handshake version byte
  when the UDP layer lands (Phase 4).
- **Velocity quantisation.** Not in this commit; velocity is derived
  server-side from inputs and may not need to be sent at all.
  Revisit when the first snapshot that actually needs it lands.
