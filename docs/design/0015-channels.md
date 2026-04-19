# 0015 — Phase 4c: channels (unreliable / reliable-unordered / reliable-ordered)

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 4 — Reliable UDP (sub-commit 4c / 6)
- **Related:** [0013 UDP socket](0013-udp-socket.md), [0014 ack + RTT](0014-ack-and-rtt.md)

## Context

Phase 4b gave us a "this packet got through" signal (`ReliableEndpoint::newlyAcked`). Phase 4c
turns that into **message-level delivery semantics** so the rest of
the engine can choose what kind of reliability it wants per payload:

- **Snapshots** are unreliable by choice — newer snapshots supersede
  older ones, losing one is not a bug.
- **Events** (you-were-eliminated, round-started) must not be lost but
  can arrive in any order.
- **Lobby / round transitions** must arrive, in order, exactly once.

## Decision

### Three channel classes, shared wire-format skeleton

```cpp
namespace engine::net {

class UnreliableChannel {
    void  send(const uint8_t*, size_t);
    size_t writeInto(BitWriter&);
    bool   readFrom(BitReader&);
    std::optional<Payload> receive();
};

class ReliableChannel {
    ReliableChannel(bool inOrder = false);
    uint16_t send(const uint8_t*, size_t);
    size_t   writeInto(BitWriter&, uint16_t packetSeq);
    void     onPacketAcked(uint16_t packetSeq);
    void     onPacketLost (uint16_t packetSeq);
    bool     readFrom(BitReader&);
    std::optional<Payload> receive();
};

}
```

Wire format per channel chunk:

- Unreliable: `varint count | { varint len, bytes }*`
- Reliable:   `varint count | { u16 msg_id, varint len, bytes }*`

Packet-level dispatch (which channel owns which bytes) is
deliberately **not** part of 4c — it lands in 4d alongside
fragmentation, when we have a full packet layout to lock down.

### Retransmit model: "in-flight tag" per message

This is the central design call. Two common shapes for reliable-UDP
retransmission:

1. **Resend everything every packet.** Simple. Bandwidth-hostile — a
   single unacked message gets re-uploaded 60 times per second.
2. **Per-message timer with RTO**. Accurate, requires a full timer
   system and a smoothed RTT estimator *plus* RTT variance. Overkill
   for Phase 4.
3. **In-flight tag (what we picked).** Each outbox message carries an
   `inFlightInPacket: optional<uint16_t>`:
   - On `writeInto(seq)`: messages with `nullopt` get packed, then
     tagged with `seq`.
   - On `onPacketAcked(seq)`: messages tagged with `seq` are removed.
   - On `onPacketLost(seq)`: tag reset to `nullopt` → available for
     retransmit on the next `writeInto`.

The loss-detection signal itself is caller-supplied. In 4c, tests
pass it directly. The engine wiring (Phase 4 closeout) will feed it
from `ReliableEndpoint`: a packet that's 32+ sequences older than the
current `mostRecentRecv_` and never acked is declared lost.

This is simple enough to hold in one's head, wasteful only if the
loss-declaration step runs late. Phase 4e (congestion control) and
later may tighten it.

### Reliable-ordered is reliable plus a holdback map

Ordered mode adds exactly one thing: a `std::map<uint16_t, Payload>`
keyed by message id. `readFrom` checks `id == expectedRecvId_`:

- equal → deliver, advance, drain contiguous run from holdback
- greater (within window) → store in holdback
- older → discard (duplicate of long-ago delivery)

The "within window" check is a wrap-aware comparison using a 2^15
distance, same pattern as `seqGreater` from 4b. Parameterising one
class via `inOrder` keeps the common code (dedup table, id
assignment, outbox handling) from being copy-pasted.

### Dedup: `unordered_map<id, bool>` for received ids

Receiver tracks every delivered id to dedup future arrivals. This
grows linearly in the session lifetime, which at say 100 reliable
messages/second × 3600 seconds = 360k entries after an hour. Not
catastrophic (under 10 MB), but ugly. A sliding-window bitset keyed
off the receiver's advancing "expected id" frontier would be the
proper fix. Parked as a Phase 4e/6 follow-up; the correct structure
is obvious, it's just not lit yet.

## Alternatives considered

1. **One `Channel` interface with a `reliability` enum.** Shorter
   header but every method grows a switch. I'd rather three classes.
   Already pays off: `UnreliableChannel::writeInto` has no `packetSeq`
   argument; it doesn't make sense for it.
2. **"Each message is its own ack."** Allocate a sequence number to
   the reliable-message channel itself and piggyback acks on channel
   traffic. Works but now you have *two* sequence spaces, and every
   channel-specific ack wastes a byte on reliable-unordered messages
   that already have the packet ack.
3. **Ordered delivery via a ring buffer of size 32.** Would match the
   packet ack width and avoid the map. But reliable-ordered can
   legitimately stall for more than 32 ids if a message genuinely
   gets wedged behind TCP-style head-of-line; the map doesn't have
   that cap. Revisit if the map shows up in profiles.

## Consequences

- Three send-receive patterns available at the call site; the engine
  never has to grow branching on "what's the reliability here" at the
  use site.
- New tests: 8 cases / 46 assertions covering FIFO delivery, ack
  drains outbox, loss triggers retransmit + receiver dedupes,
  in-flight tag prevents duplicate sends without a loss signal,
  holdback-then-release in ordered mode, dedup of in-holdback ids.
- Totals: **56 cases / 305 assertions green.**
- `simulation` library still GL-free. `engine::net` surface is now:
  `BitStream`, `Quantize`, `Socket`, `PacketHeader`, `SequenceBuffer`,
  `ReliableEndpoint`, `Channel`.

## Code pointers

- [include/engine/net/Channel.h](../../include/engine/net/Channel.h)
- [src/engine/net/Channel.cpp](../../src/engine/net/Channel.cpp)
- [tests/test_channel.cpp](../../tests/test_channel.cpp)

## Follow-ups (explicit)

- **MTU enforcement in `writeInto`.** Today it writes everything in
  the outbox. Once Phase 4d lands a per-packet budget, `writeInto`
  learns a `maxBytes` argument and leaves the rest in the outbox.
- **Loss detection wiring.** `onPacketLost` is a public call that nobody
  calls yet; hooking it to `ReliableEndpoint`'s receive-window advance
  is a Phase 4 closeout task.
- **Bounded dedup window.** Replace `unordered_map<id, bool>` with a
  sliding bitset keyed off the receive frontier.
- **Reliable channel fragmentation.** A single reliable message larger
  than one packet needs to be split. Explicitly Phase 4d.

## Next

- 4d: fragmentation. Split a payload > MTU into N fragments; reassemble
  on receive; drop on timeout.
