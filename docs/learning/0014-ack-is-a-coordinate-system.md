# 0014 — The ack bitfield is a coordinate system

- **Date:** 2026-04-19
- **Commit:** Phase 4b — packet header + ack + RTT
- **Related:** [design/0014](../design/0014-ack-and-rtt.md), [learning/0013](0013-smallest-socket.md)
- **Principles:** One primitive for lookalike state, off-by-one lives in the gap, clocks are dependencies

---

## What I built

A 12-byte packet header, a generic `SequenceBuffer<T, N>` sliding
window, and a `ReliableEndpoint` that combines the two to produce
(a) stamped outbound headers and (b) a list of newly-acked sequences
every time an inbound header arrives. Plus a smoothed RTT in the low
double digits of lines of code.

11 new tests, 92 new assertions.

## Why (the problem)

Last commit I had a socket. This commit I had to turn a dumb datagram
pipe into something the rest of the engine can *reason about*. Three
things had to become answerable:

1. Did the peer receive packet N?
2. What's the round trip to the peer right now?
3. If packet N got lost, when/how do we notice?

All three hang off the same machinery: sequence numbers with an ack
bitfield. So that's what this commit is.

## How I approached it

### Realisation 1: inbound and outbound state are the same shape

First cut of the design had two sibling classes: `SentPacketTracker`
and `ReceivedPacketTracker`. Both were ~50 lines. Both were doing the
same thing: "given a 16-bit sequence, O(1) lookup whether we know
about it and what we know."

Stopped. Deleted both. Wrote one template — `SequenceBuffer<T, N>` —
parameterised by the per-slot payload type (`SentRecord` vs
`RecvRecord`). The endpoint has two instances of the same thing.
Less code, one concept to test, one place for a bug in the
slot-collision logic to hide.

Rule from this: **when two state-tracking classes share a shape,
they're the same class with different T.** The instinct to "keep them
separate so each has a clear purpose" is a straightforward trap; the
*purpose* is the index-by-sequence-number part, and that's already
one concept.

### Realisation 2: RFC-1982 wrap-around isn't optional

At 60 Hz with 16-bit sequences you wrap every 18 minutes. A game
session routinely runs longer than that. Comparing sequences with
naive `<` means at minute 18 every "is this newer?" check inverts.

This is the kind of bug that a) won't happen in Debug for the first
run of the game, b) won't happen in any test under 18 minutes,
c) *will* happen in production during the first competitive match
that goes into overtime. The only honest way to handle it is to
funnel every comparison through one function — `seqGreater(a, b)` —
and never write raw `<` on a sequence value anywhere in the codebase.

Noted in the header comment. Treat it like we treat `size_t` vs `int`:
one well-defined operation, don't invent local ones.

### Realisation 3: time is a dependency, not a global

The naive way to write this endpoint is to call
`std::chrono::steady_clock::now()` everywhere time is needed. Works,
until you want to unit-test "if the peer acks after 30 ms, srtt
should be 30." Then you need to sleep for 30 ms, which is slow, flaky,
and a lie because the real path won't look like `sleep_for`.

Instead: pass a `const Clock&` in the constructor. Provide a
`SteadyClock` for production, a `FakeClock` in tests with
`advance(ms)`. The RTT test does `clk.advance(30)` between send and
ack and asserts `srtt ≈ 30.0`. Fast, exact, intentional.

This is broadly applicable. Any class whose behaviour depends on
elapsed time should take a `Clock`. The cost is one extra ctor
parameter; the benefit is every timing scenario becomes a pure unit
test.

## The bug I nearly shipped

Writing the "packets 0..4 sent, packet 2 dropped" test revealed a
subtle one: when `mostRecentRecv_` jumps forward (say from 4 to 10),
the slots in between *might* still contain stale sequence numbers from
a previous lap around the ring buffer (seq 0 and seq 1024 share a
slot). If I didn't explicitly scrub slots 5..9, the next
`computeOutboundAckBits` call would find seq 8 sitting in slot 8
(actually owning it from the last lap) and report ack-bit for 8 even
though we never saw it *this* lap.

1024 slots vs 18 minutes of traffic = this bit is 1024 packets = 17
seconds of collision-free lookback before lap-around becomes
relevant. Test-only, in practice, until someone disconnects and
reconnects in the same session. Still wrong. Fixed with an explicit
gap scrub on window advance.

Finding it by writing the dropped-middle test, not by writing the
happy-path test, is a reminder that **adversarial tests find more
bugs than more happy-path tests do**.

## Principle(s) this demonstrates

- **Unify lookalikes.** Two 50-line classes differing only in payload
  type are one template with different Ts.
- **Sequence arithmetic is a custom operator.** Never `<`. Route
  everything through `seqGreater`. The compiler won't save you here.
- **Inject time.** Classes that behave differently in 30 ms should be
  testable by asserting that fact in microseconds.
- **Write the adversarial test before you claim the feature works.**
  The happy path is nearly always right by the time you run it. The
  edge cases are where the commits happen.

## What I got wrong first

- My first cut had two separate classes. Caught in review-of-self
  before it compiled.
- First version of `seqGreater` triggered `-Wsign-compare` because
  u16 − u16 promotes to `int`, not unsigned. Fixed by casting both
  operands to `uint32_t` before the subtract. Sign-promotion rules in
  C++ remain a persistent foot-gun; CI catches it, which is the only
  reason CI exists.
- Initial "ackBits" test wasn't sorting the returned sequences before
  comparing — order is unspecified. Caught by adding `std::sort`.
- Wanted to return `std::vector<uint16_t>` by value from
  `processInboundHeader`. Caller-owned `vector<>&` saves one malloc
  per packet, per peer, per tick. Kept it.

## Takeaway for future me

- When you find yourself writing parallel sibling classes, stop and
  ask if there's a template in the middle.
- Pluggable clock + fake clock + `advance(ms)` is the standard
  recipe for testing time-dependent code. Do it from day one; don't
  retrofit.
- Adversarial tests first. "Works on happy path" passes so reliably
  it's almost not informative.
- Every new TU in the engine now starts with `PacketHeader` on its
  wire. Good time to make sure the header's offsets are frozen forever
  — future additions go after byte 12, or bump `kProtocolId`.
