# 0016 — Phase 4d: fragmentation & reassembly

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 4 — Reliable UDP (sub-commit 4d / 6)
- **Related:** [0015 channels](0015-channels.md), [0014 ack + RTT](0014-ack-and-rtt.md)

## Context

UDP datagrams larger than the path MTU get dropped or IP-fragmented,
and IP-fragmentation is famously flaky in the wild — once one IP
fragment is lost, the whole UDP datagram is lost. Every production
game engine solves this at the application layer: if your payload
is bigger than ~1200 bytes, you split it yourself with a fragment
index and total, retransmit on the fragments individually, and
reassemble on the receive side.

Phase 3 produced potentially-large snapshots (worst case 15.9 KB per
the benchmarks in [0012](0012-benchmarks.md)). Phase 4c channels
assume one message fits in one packet. 4d closes the gap.

## Decision

### Surface

Two pieces, both stateless-ish from the outside:

```cpp
struct FragmentHeader {
    uint16_t messageId;   // unique per fragmented message
    uint16_t fragIndex;   // 0..fragTotal-1
    uint16_t fragTotal;   // total fragments in this message
};

std::vector<FragmentPayload> fragment(
    uint16_t msgId, const uint8_t*, size_t, size_t maxFragBody);

class Reassembler {
    std::optional<FragmentPayload> offer(const uint8_t*, size_t, Clock::Millis);
    void gc(Clock::Millis, Clock::Millis timeoutMs);
};
```

Six-byte fragment header, big-endian, following the same discipline
as `PacketHeader` from 4b. A fragment datagram = `FragmentHeader`
followed by up to `maxFragBody` bytes of payload.

### No fragment-level acks

The ack mechanism lives in `ReliableEndpoint` (per packet) and
`ReliableChannel` (per message). Fragments inherit both:

- A fragmented reliable message, once fully reassembled, is
  `readFrom`'d into `ReliableChannel`, which dedups on message id.
- A fragmented *unreliable* message (snapshots) is fire-and-forget —
  a single lost fragment drops the whole snapshot and the next one
  supersedes it.

Adding explicit per-fragment retransmission here would require each
fragment to carry its own packet sequence, which means each fragment
would effectively become its own reliable "message" and we'd be
reinventing `ReliableChannel` inside `Reassembler`. Hard no.

The cost: a lost fragment in a 16-fragment reliable message causes
retransmission of all 16. With per-fragment acks we'd retransmit
only one. At 5% packet loss and 16 fragments, the expected
retransmission count is ~2.4× with all-or-nothing vs ~1.05× with
per-fragment acks. That's meaningful for large reliable messages,
but the code complexity of per-fragment retransmission is large. I'm
parking this. If ranked-score / replay files / big lobby-state pushes
become a real workload it's worth revisiting.

### Safety caps

- **`kMaxFragmentsPerMessage = 256`.** A single message bigger than
  256 × 1200 B ≈ 300 KB is almost certainly a bug; `fragment()` returns
  `{}` above this.
- **`kMaxInFlight = 64`.** The reassembler holds at most this many
  partial messages; when a 65th shows up, the oldest is evicted. This
  is the anti-DOS knob: without it, a peer could send one fragment
  each of 65 535 distinct `messageId`s and make us buffer forever.
- **`kDefaultTimeoutMs = 2000`.** Partial messages older than this
  get dropped on a caller-driven `gc()`. 2 s is generous for normal
  play; a tighter bound could catch malice faster but risks
  false-positive drops under heavy congestion.

All three numbers are named constants in the header so reviewers see
them and future tuning is a one-line change.

### Injected clock, caller-driven `gc()`

`offer()` takes `nowMs` directly; `Reassembler` never calls `now()`
itself. Same rule as 4b: **time is a dependency**. Makes every test
deterministic without `sleep_for`. The outer layer (the per-tick net
pump) is responsible for calling `gc()` periodically — probably once
per tick, which is cheap.

### fragTotal is a schema contract

Once the first fragment for a given messageId arrives, `fragTotal`
is committed. Any later fragment with the same id but a different
`fragTotal` is treated as a protocol violation and the whole
in-flight entry is dropped. This is stricter than it needs to be (a
delayed-then-resent-with-different-framing message *could* change
fragTotal), but "fragmentation framing is stable for the lifetime of
a messageId" is an invariant I want the engine to rely on.

## Alternatives considered

1. **IP-layer fragmentation (let the kernel do it).** The well-known
   failure mode. One lost IP fragment → dead UDP datagram, with no
   signal. Rejected by the entire game-netcode literature.
2. **Per-fragment acks** (as above). Better worst-case retransmission
   count, at meaningful complexity cost. Parked.
3. **Variable-size header with a 1-byte form for `fragTotal == 1`.**
   Saves 5 bytes on the common case where there's no fragmentation.
   Attractive but messy at the boundary — `readFragmentHeader` now
   needs to probe the first byte. I think we should instead skip the
   fragment wrapper entirely when the payload fits in one packet,
   which is a higher-layer decision (4-closeout). Kept the header
   fixed-size here.
4. **Store fragments as one contiguous buffer indexed by offset.**
   Saves the per-fragment `vector<uint8_t>` allocation. Small win,
   but requires the first fragment's `fragTotal` × `maxFragBody` to
   pre-size — and an attacker could trick us into pre-allocating
   300 KB per incoming `messageId` by claiming 256 × 1200. Per-piece
   allocation is safer today.

## Consequences

- `engine::net` now has everything needed to carry arbitrarily-sized
  messages. The missing step is the outer glue: a thin per-peer class
  that holds a `UdpSocket`, a `ReliableEndpoint`, one or more
  `Channel`s, and a `Reassembler`, and pumps bytes between them. That's
  Phase 4 closeout.
- 10 new tests (63 assertions) covering header round-trip, even/uneven
  split, safety-cap refusal, in-order reassembly, shuffled
  reassembly, duplicate-fragment tolerance, malformed-input rejection,
  gc expiry, fragTotal-contract violation.
- Totals: **66 cases / 368 assertions green.**
- `simulation` target still GL-free.

## Code pointers

- [include/engine/net/Fragmentation.h](../../include/engine/net/Fragmentation.h)
- [src/engine/net/Fragmentation.cpp](../../src/engine/net/Fragmentation.cpp)
- [tests/test_fragmentation.cpp](../../tests/test_fragmentation.cpp)

## Next

- 4e: congestion control. The trigger is an EWMA on RTT vs a
  threshold — when RTT spikes, drop the send rate (Gaffer "good mode
  / bad mode"). This is also when `bandwidthCap` starts feeding the
  per-packet budget that `Channel::writeInto` will consume.
