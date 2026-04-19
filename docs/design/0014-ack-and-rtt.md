# 0014 — Phase 4b: packet header, ack bitfield, and smoothed RTT

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 4 — Reliable UDP (sub-commit 4b / 6)
- **Related:** [0013 UDP socket](0013-udp-socket.md), [0011 delta encoding](0011-delta-encoding.md)

## Context

Phase 4a gave us a non-blocking UDP socket. Datagrams go out, datagrams
come back. But we have no way of knowing *which* datagrams the peer
received, so:

- Reliable channels are impossible (don't know what to retransmit).
- RTT is unknown (can't time anything).
- Delta snapshots have no honest baseline (we don't know what the
  peer has).

4b adds the minimum machinery for all three: a 12-byte packet header,
a 32-bit ack bitfield, and a smoothed RTT estimator.

## Decision

### Header: 12 bytes, big-endian

```
offset  size  field          purpose
0       4     protocolId     magic; reject foreign/old-version pkts
4       2     sequence       our packet number
6       2     ack            peer's most recent seq we received
8       4     ackBits        bitfield of 32 preceding acked seqs
                             bit i = 1 iff (ack - 1 - i) was received
```

Fixed, not bit-packed, not varint. These fields are on the critical
read path of every datagram and need trivial, branch-free (de)serialise.
`kProtocolId = 'MENG'` (0x4D454E47) — arbitrary but greppable in
packet captures.

**Big-endian on the wire.** No host-order leakage even on x86-only
deployments. The public `Endpoint` in 0013 is host-order because it
never touches the wire; the packet header DOES touch the wire, so it's
network-order throughout.

Future layers (channels, fragmentation) will add fields AFTER byte 12
in a bumped protocol id; offsets 0–11 are now stable forever.

### Sequence arithmetic: RFC-1982 on 16 bits

16-bit sequences wrap every 65 536 packets. At 60 Hz that's ~18 minutes.
Comparing with `<` is wrong once you wrap; `seqGreater(a, b)` implements
the classic "within half the window" rule. All sequence comparisons in
the engine go through this one function.

### `SequenceBuffer<T, N>` — single sliding-window primitive

Both outbound ("have we been acked yet?") and inbound ("did we get
this seq?") state are the same shape: a sparse map keyed by 16-bit
sequence, only meaningful for the most recent ~1 K entries. Instead of
writing two lookalike classes I wrote one header-only template
(`SequenceBuffer<T, N=1024>`):

- Slot index = `seq & (N-1)`.
- Parallel `seqAt_` array tracks "which seq currently owns this slot"
  — on insert we overwrite, on find we verify.
- `N=1024` ≫ the 32-wide ack window, so the buffer can't forget a
  packet before an ack has had a chance to reach us.

### `ReliableEndpoint` — the ack/RTT brain

```cpp
class ReliableEndpoint {
    std::uint16_t writeOutboundHeader(uint8_t* out, size_t cap);
    bool processInboundHeader(const PacketHeader& h,
                              std::vector<uint16_t>& newlyAcked);
    double smoothedRttMs() const;
};
```

One instance per peer. `writeOutboundHeader` assigns the next
sequence, records the send time, and stamps the header with the
current `(ack, ackBits)`. `processInboundHeader` updates the receive
window, then walks `ack` + the 32 bits in `ackBits` against our sent
buffer — any newly-confirmed sequence yields an RTT sample.

**RTT smoothing: EWMA with α=0.1.** `srtt = 0.9 * srtt + 0.1 *
sample`. Gaffer's suggestion, simpler than Jacobson/Karels and
entirely adequate for a game engine where we're feeding SRTT into
jitter buffers and congestion control, not TCP retransmit timers.
First sample bootstraps `srtt` directly (no ramp-up artefact).

**Does NOT own a socket.** The class takes bytes in and bytes out.
Mocking time via a pluggable `Clock` abstraction makes every scenario
deterministic in tests. Plumbing `UdpSocket` into `ReliableEndpoint`
is a later commit (probably 4c).

### Duplicate / stale / out-of-order handling

- Duplicate inbound (seq already in the receive buffer, or equal to
  `mostRecentRecv_`): dropped, no ack effect.
- Out-of-order but within 32 of `mostRecentRecv_`: accepted, seeds
  the ackBits we emit next.
- Older than 32: dropped. The ack window can't express it, so
  "receiving" it would lie to the peer.
- Newer than `mostRecentRecv_`: advances the window. The implementation
  explicitly scrubs any gap slots that might collide with very old
  sequences (1024-slot ring means seq N and seq N+1024 hash to the
  same place) — without this, `computeOutboundAckBits` could report a
  spurious 1 for a packet we never actually received. Found this while
  writing the "dropped-in-middle" test.

## Alternatives considered

1. **32-bit sequence numbers.** Eliminates wrap-around concerns but
   doubles the sequence field width. Not worth 2 bytes per packet
   forever to avoid writing one 3-line comparator.
2. **Per-packet ack (no bitfield), rely on multiple packets per tick
   to cover losses.** Simpler but every lost ack-carrying packet
   forgets up to 32 acks permanently. Gaffer's analysis is conclusive;
   we're not redoing it.
3. **Carry RTT calculation in the inbound path of the *application*
   code, not the endpoint.** Mixes concerns. The endpoint owns the
   send-timestamps and the ack semantics; pushing the EWMA out means
   leaking both.
4. **TCP SRTT (Jacobson/Karels with RTTVAR).** Useful if we were
   computing retransmission timeouts. We aren't — reliable channels in
   4c use per-message deadlines keyed to `smoothedRttMs()`, not a
   full RTO estimator. Revisit if reality disagrees.

## Consequences

- Every future packet the engine emits starts with 12 bytes of
  `PacketHeader`. The snapshot payload from Phase 3 becomes the body.
- `SequenceBuffer` is a reusable primitive — it'll also back the
  reliable-ordered channel's out-of-order reassembly queue in 4c.
- 11 new tests (92 assertions) exercise header round-trip, protocol-id
  rejection, 16-bit wrap, ack-bitfield accounting across happy path,
  dropped-middle, and duplicate reject, and RTT smoothing across
  multiple samples.
- Total: **48 cases / 259 assertions green.**
- `simulation` target still compiles with zero GL dependencies.

## Code pointers

- [include/engine/net/PacketHeader.h](../../include/engine/net/PacketHeader.h)
- [src/engine/net/PacketHeader.cpp](../../src/engine/net/PacketHeader.cpp)
- [include/engine/net/SequenceBuffer.h](../../include/engine/net/SequenceBuffer.h)
- [include/engine/net/ReliableEndpoint.h](../../include/engine/net/ReliableEndpoint.h)
- [src/engine/net/ReliableEndpoint.cpp](../../src/engine/net/ReliableEndpoint.cpp)
- [tests/test_reliable_endpoint.cpp](../../tests/test_reliable_endpoint.cpp)

## Next

- 4c: channels. Unreliable (snapshots pass through), reliable-unordered
  (retransmit until acked), reliable-ordered (buffer+deliver in sequence).
  Built on top of `ReliableEndpoint`'s newly-acked list.
