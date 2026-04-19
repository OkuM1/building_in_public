# 0008 — `engine::net::BitStream`: bit-level serialization primitive

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 3 (Serialization & snapshots) — Commit 1/5
- **Related:** [../ROADMAP.md § Phase 3](../ROADMAP.md#phase-3--serialization--snapshots)
- **Supersedes:** —

## Context

Phase 3's goal is compact world snapshots (target: < 4 KB for 1k
entities, < 200 B typical delta). That pressure only gets hit by
packing each field to the minimum number of bits it needs — 16 bits
for a quantised position, 8 for an angle, 1 for a bool, not a padded
`float[3]` per component. That requires bit-level I/O, not a byte
stream.

This commit adds that primitive and nothing else. Every later Phase 3
commit (quantised types, snapshot encoder, delta encoder) reads and
writes through this file.

## Decision

Two classes in `engine::net`:

- **`BitWriter`** — owns a `std::vector<uint8_t>`, accumulates bits
  MSB-first, grows the buffer as needed. No bounds-check on write.
- **`BitReader`** — borrows a `(const uint8_t*, size)`, consumes in
  the same order. Read-past-end flips an internal `ok_` flag; further
  reads return 0. Callers check `ok()` once at end of decode.

Primitives:

| API | Bit width | Notes |
|---|---|---|
| `writeBits(value, n)` | 1..32 | the workhorse |
| `writeBool` | 1 | |
| `writeUint8/16/32`, `writeInt32`, `writeFloat` | 8/16/32 | auto-align to byte boundary first |
| `writeVarint` | 8..40 | byte-aligned LEB128; 7 data bits + continuation |
| `align()` | — | pad to next byte with zeros |

Storage convention: **big-endian bit order** (MSB first within each
byte). This matches network protocol convention and keeps a human
hex-dump readable left-to-right. Endianness of multi-byte integers is
also big-endian on the wire — we write `uint32_t` as four consecutive
`writeBits(_, 8)`s from high byte to low.

Float serialization copies raw 32 bits via `memcpy` (avoids the
type-pun UB). Cross-host endianness handling is deferred to Phase 4
when the UDP socket layer lands; until then the client and server run
on the same machine.

## Alternatives considered

1. **Byte-stream only (no bit packing).** Simpler, but wastes ~75% of
   the wire for a quantised position. Fails the Phase 3 size target.
   Rejected.
2. **Template-based `Stream<Mode>` à la Glenn Fiedler.** One class
   with a compile-time read/write mode. Cute for shared serialize
   functions, but compiler errors balloon and tests become awkward.
   We can add a thin `serialize(Stream&, T&)` traits layer on top in
   Commit 3 without committing to a single-class model now. Rejected
   for the primitive.
3. **Google's Protobuf / Cap'n Proto.** They'd handle this and much
   more, but the point of the repo is to understand and benchmark
   netcode from the bytes up. Rejected for learning reasons.
4. **`std::bitset` / `std::vector<bool>`.** Wrong layer — these are
   bit *containers*, not *streams*. No write-cursor, no byte output.
   Rejected.

## Consequences

- **Enables** — quantised component serializers in Commit 2,
  full-world encode in Commit 3, baseline-ack deltas in Commit 4.
- **Accepts** — floating-point and multi-byte int serialization is
  endianness-fragile until Phase 4 adds byte-swap helpers on the
  socket path.
- **Costs** — one small TU added to the `simulation` target. Zero
  external deps. Build is still GL-free on the `simulation` target.

## Code pointers

- [include/engine/net/BitStream.h](../../include/engine/net/BitStream.h)
- [src/engine/net/BitStream.cpp](../../src/engine/net/BitStream.cpp)
- [tests/test_bitstream.cpp](../../tests/test_bitstream.cpp) — 6 cases,
  21 assertions covering arbitrary-width roundtrip, typed roundtrip,
  varint length bounds, varint roundtrip across the full `uint32_t`
  range, read-past-end safety, and interleaved bit/byte alignment.

## Open questions

- Should `writeVarint` zigzag-encode signed inputs? Deferred: none of
  the planned Phase 3 component fields are signed varints. Revisit in
  Commit 2 if a signed quantised type wants it.
- `BitWriter::finish()` returns a `const&` to the internal buffer.
  When the socket layer arrives we may want a `std::span<const uint8_t>`
  return instead. Revisit in Phase 4.
