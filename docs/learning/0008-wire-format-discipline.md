# 0008 — Wire-format discipline: build the byte layer before the protocol

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 1 of Phase 3)*
- **Related:** [design/0008](../design/0008-bitstream.md)
- **Principles:** YAGNI (but inverted), build bottom-up when the bottom is the thing that constrains you

---

## What I built

`engine::net::BitStream` — a `BitWriter` that packs 1–32 bits at a
time MSB-first into a growing `std::vector<uint8_t>`, and a matching
`BitReader` that consumes the same format. Plus byte-aligned typed
writes (`u8/u16/u32/i32/float`) and a LEB128 varint. No snapshot
code, no network code, no component serializers. Just the primitive.

## Why (the problem)

Phase 3's hard constraint is a size target: 1k entities in < 4 KB,
typical delta < 200 B. You do not hit those numbers with a byte stream
and `std::memcpy`. You hit them by packing each field to the minimum
bits it needs — a quantised Y-coordinate in 14 bits, an angle in 8, a
"has velocity" flag in 1. That shape of work requires bit-level I/O
to even be expressible. So the primitive has to land before anything
that uses it.

The temptation was to skip straight to "snapshot encoder" and build
the bit ops inline, then refactor later. Resisting that was the whole
lesson of this commit.

## How I approached it

I wrote the header first, then the tests, then the implementation.
Header-first forces you to stare at the public API for a minute
before any code commits you to it. Questions that surfaced from just
sitting with the header:

- Does the writer bounds-check? (No. Grows.)
- Does the reader? (Yes — flips an `ok` flag, subsequent reads
  return 0, callers check once.)
- MSB-first or LSB-first within a byte? (MSB — hex-dump readable,
  matches every protocol spec I've implemented against.)
- Are typed writes bit-packed or byte-aligned? (Byte-aligned. The bit
  packing is for explicit `writeBits(n)` calls; typed writes are for
  when you want convenience and can afford the padding.)
- Varint byte-aligned or bit-level? (Byte-aligned. LEB128 as usually
  specified.)

Every one of those is the kind of question that, unanswered, would
have bitten me three commits from now when the snapshot format hits
a wall. Answering them in the header — with a test per answer —
costs an hour now and zero hours later.

## Principle(s) this demonstrates

- **Bottom-up when the bottom constrains you.** Most of the time
  top-down is the right default: write the caller, let it drive the
  API of the callee. But when a numeric constraint (bandwidth
  budget) forces the shape of the primitive, the primitive has to
  come first. Writing the snapshot encoder top-down would have
  produced a byte-stream API that the encoder then couldn't hit the
  budget with, and I'd have rewritten the primitive anyway.
- **YAGNI, inverted.** The companion to "don't build what you don't
  need yet" is "when you *do* need it, build the minimum, alone,
  with its own tests, and then stop." This commit adds zero speculative
  features — no zigzag varint, no `std::span` overload, no
  compression, no checksum — even though I could argue for each.
  Each is a decision for the commit that proves it's needed.
- **Test the primitive at the primitive's level.** The tests don't
  decode "a snapshot"; they decode "3 bits then 16 bits then an
  unaligned tail." If that fails, I don't want to be debugging
  through a snapshot encoder to find it.

## What I got wrong first

- First draft of `writeBits` had `scratch_` as a `uint32_t`. That
  meant writing 24 pre-existing bits and then a 32-bit value
  overflowed the accumulator mid-call. Switched to `uint64_t` so one
  `writeBits(_, 32)` call always fits regardless of `scratchBits_`.
  This is the kind of bug that's obvious in code review and invisible
  in runtime — exactly why the primitive earns its own tests.
- The first `readBits` tried to be clever with a 64-bit sliding
  window. It worked but it was unreadable. I replaced it with an
  obvious byte-at-a-time loop. Network primitives live forever; pick
  the readable version unless a benchmark says otherwise.

## Takeaway for future me

- When a phase has a numeric contract (bytes/sec, entities/frame,
  ms/tick), the commits should stack in the order that lets you
  measure. BitStream → quantised types → encoder → benchmark. Each
  commit's test should already be measurable against the ultimate
  budget, even if approximately.
- Header-first for low-level primitives. Tests-second. Implementation
  third. The order matters: it's the difference between designing the
  API and stumbling into it.
- Every design note should enumerate **alternatives considered** and
  why each was rejected — not because the reader needs it, but
  because it's the cheapest way to expose a decision that hasn't
  actually been made yet. I almost shipped a varint with no zigzag
  support having never asked the question. Writing "zigzag: deferred
  because no signed varints on the Phase 3 field list" forced the
  field-list check that confirmed it.
