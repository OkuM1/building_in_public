# 0019 — Phase 4 closeout: Connection glue facade

- **Status:** Implemented
- **Date:** 2026-05-07
- **Phase:** 4 — Reliable UDP (closeout / glue commit)
- **Related:** [0013 UDP socket](0013-udp-socket.md), [0014 ack + RTT](0014-ack-and-rtt.md),
  [0015 channels](0015-channels.md), [0016 fragmentation](0016-fragmentation.md),
  [0017 congestion](0017-congestion-control.md), [0018 network simulator](0018-network-simulator.md)

---

## Context

Phases 4a–4f produced six independently tested primitives:

| Sub-commit | Class | Role |
|---|---|---|
| 4a | `UdpSocket` | Non-blocking UDP I/O |
| 4b | `ReliableEndpoint` | Ack + RTT tracking |
| 4c | `UnreliableChannel` / `ReliableChannel` | Message delivery semantics |
| 4d | `Fragmentation` / `Reassembler` | MTU-safe message splitting |
| 4e | `CongestionController` | Send-rate gating |
| 4f | `SimulatedLink` | In-process adversarial link |

None of them talked to each other. `ReliableChannel::onPacketLost` was a
public method nobody called. The congestion controller had no link to the
endpoint's RTT. Tests hand-wired fragments of the stack manually.

The ROADMAP Phase 4 milestone requires:

> *Two processes exchange reliable + unreliable messages over loopback
> with injected 150 ms / 5 % loss and remain stable.*

To get there we need a single class that assembles the primitives into a
working per-peer facade.

Additionally, `ReliableEndpoint::processInboundHeader` emitted acked
sequences but not *lost* sequences. Loss detection had to be plumbed in
so `ReliableChannel::onPacketLost` actually fires.

---

## Decision

> **Add `engine::net::Connection` — a per-peer facade — and extend
> `ReliableEndpoint` with packet-loss detection.**

### Loss detection in `ReliableEndpoint`

A new two-vector overload:

```cpp
bool processInboundHeader(const PacketHeader& h,
                          std::vector<uint16_t>& newlyAcked,
                          std::vector<uint16_t>& newlyLost);
```

After resolving acks via the existing `resolveAcks()` path, this overload
scans 64 slots beyond the 32-slot ack window (`h.ack - 33` to
`h.ack - 97`). Any sent sequence found in `sent_` with `acked=false` is
outside the peer's ack window permanently — it is appended to `newlyLost`
and removed from `sent_`. The scan is O(64) per received packet and
idempotent (removed entries are never found twice).

The original single-vector overload is unchanged; all existing tests
continue to compile without modification.

### `Connection` packet wire format

Every packet is a flat byte buffer with no inter-section delimiters:

```
[ 12 bytes PacketHeader        ]   Phase 4b
[ channel-0 section (snapshot) ]   varint count | {varint len, bytes}*
[ channel-1 section (events)   ]   varint count | {u16 id, varint len, bytes}*
[ channel-2 section (lobby)    ]   varint count | {u16 id, varint len, bytes}*
```

Each channel section is self-framing. The receiver parses exactly the
message count specified by the leading varint, then moves to the next
section. No length prefix on the section itself; no channel-id byte
needed since the three sections always appear in the same fixed order.

### `Connection` public API

```cpp
class Connection {
public:
    explicit Connection(const Clock&, CongestionConfig = {});

    void send(ChannelId ch, const uint8_t* data, size_t n);
    bool shouldSendNow(Clock::Millis nowMs) const;
    std::vector<uint8_t> buildPacket(Clock::Millis nowMs);

    bool receivePacket(const uint8_t* data, size_t n, Clock::Millis nowMs);
    std::optional<Payload> receive(ChannelId ch);

    double         smoothedRttMs()    const;
    CongestionMode congestionMode()   const;
};
```

`buildPacket` always produces a valid packet even when all channels are
empty: the header carries acks back to the peer so their loss-detection
can advance.

`shouldSendNow` checks a `nextSendMs_` gate updated by the congestion
controller after each `buildPacket`. Initialized to 0 so the first send
fires immediately.

---

## Alternatives considered

- **Separate `buildHeader` and `buildPayload` calls.** The caller would
  have to manage sequence-number consistency between them. Single
  `buildPacket` keeps that invariant inside the class.

- **MTU budget passed into `buildPacket`.** Phase 4d's `Fragmentation`
  exists but is not wired here. Doing so would also require the reliable
  channels to learn a `maxBytes` argument for `writeInto`. Parked — the
  channels currently write their entire outbox into each packet, which is
  fine for the message sizes in this engine.

- **Fixed-order vs. channel-id-tagged sections.** Tagged sections cost
  1–2 extra bytes per packet and add a dispatch branch on receive. Since
  the number of channels is fixed and stable, fixed order is simpler with
  no downside.

---

## Consequences

**Positive**
- Phase 4 milestone achieved: two `Connection` objects over two
  `SimulatedLink`s at 150 ms RTT / 5 % loss deliver all 100 reliable
  messages and maintain a measured RTT estimate.
- `ReliableChannel::onPacketLost` is finally called — retransmission
  works end-to-end.
- RTT feeds into `CongestionController::update` on every received packet.
- Single-call API for the server's per-tick net pump: `shouldSendNow` →
  `buildPacket` → transmit → `receivePacket` → `receive`.

**Negative / cost**
- `Fragmentation` is not wired in. Large messages (> ~1400 bytes) are
  split across multiple `send()` calls by the caller or will overflow
  one packet. Acceptable for Phase 5 (snapshot sizes are in Phase 3's
  measured 82 B range).
- Loss-detection scan depth (64 slots) is a heuristic. A sender that
  idles for > 64 packets' worth of wall time before the peer responds
  would not get retransmit notifications for old sequences. Pathological
  case only.

**Follow-ups**
- Wire `Fragmentation` into `ReliableChannel::writeInto` when a message
  exceeds the per-packet budget.
- Phase 5: `Connection` is the seam the replication model sits on. The
  server will hold one `Connection` per client.

---

## Code pointers

- [`include/engine/net/Connection.h`](../../include/engine/net/Connection.h)
- [`src/engine/net/Connection.cpp`](../../src/engine/net/Connection.cpp)
- [`include/engine/net/ReliableEndpoint.h`](../../include/engine/net/ReliableEndpoint.h)
- [`src/engine/net/ReliableEndpoint.cpp`](../../src/engine/net/ReliableEndpoint.cpp)
- [`tests/test_connection.cpp`](../../tests/test_connection.cpp)
