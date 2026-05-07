# 0020 — Phase 5a: Replication Model

> **Date:** 2026-05-07
> **Status:** implemented
> **Phase:** 5a — Server-authoritative loop + snapshot interpolation + prediction + reconciliation

---

## Context

Phase 4 produced `engine::net::Connection` — a per-peer UDP facade with three
channels (unreliable snapshots, reliable-unordered events, reliable-ordered
lobby). The snapshot encoder (`Serialize.h`) and quantiser (`Quantize.h`) were
landed in Phase 3.

Phase 5 sits entirely above the transport. It never touches sequence numbers or
ack bitmasks. Its job is to answer the question: *given that we can push bytes
reliably and unreliably between two processes, how do we run a single
authoritative simulation and make every client feel responsive?*

This note covers the Phase 5a deliverables:

- `engine::replication::InputMessage` — client→server input wire format
- `engine::replication::SnapshotBuffer` — client-side interpolation ring buffer
- `engine::replication::PredictionBuffer` — client-side prediction + reconciliation buffer

---

## Decisions

### D1: InputMessage carries the client tick

Every `InputMessage` includes the sender's simulation tick. The server uses this
to:

1. **Schedule** the input at the correct simulation step (when it arrives early).
2. **Echo** the last processed input tick back in snapshots, so the client knows
   exactly how far ahead its prediction runs relative to the server's confirmed
   state.

Alternative: send *only* inputs without a tick. Rejected: makes lag compensation
impossible and forces the server to treat every input as "now".

### D2: SnapshotBuffer stores decoded EntityState, not raw bytes

Two options:

| Option | Pro | Con |
|---|---|---|
| Store raw snapshot bytes | Compact, lazy decode | Must decode twice per interpolation step (once per bounding snapshot) |
| Store decoded EntityState | Decode once per snapshot | More memory per entry |

We chose decoded `EntityState` (`EntityId + Transform`). Memory is trivial:
16 bytes × 16 entities × 16 buffer slots = 4 KB. Decode-once avoids doing
BitStream arithmetic on every render frame.

Only `Transform` components are stored in `EntityState`. Velocity, PlayerInput,
and other components are applied to the world directly on snapshot receipt, not
interpolated. This matches how Source/Quake handle it — you interpolate *position*
and *orientation* because they're continuous; discrete state (speed, input) is
applied as-is.

### D3: SnapshotBuffer uses oldest-first deque with capacity eviction

A `std::deque<ReplicaSnapshot>` capped at `kCapacity = 16` entries. Pushes
append to the back; full-buffer eviction pops from the front. Out-of-order
snapshots (tick ≤ newest buffered) are discarded.

Alternative: a circular array keyed by `tick % capacity`. Rejected: requires
handling wrap-around edge cases for a very small gain. The deque approach is
transparent in a debugger and correct under jitter.

### D4: PredictionBuffer caps at 128 ticks

At 60 Hz, 128 ticks ≈ 2.1 s of unacknowledged history. Any realistic RTT
(< 300 ms → 18 ticks) fits with massive headroom. When the buffer is full
the oldest entry is silently evicted — the client will reconcile against the
server's last confirmed tick rather than the mispredicted one, which is still
correct (just slightly less precise).

Alternative: dynamic size (never evict). Rejected: unbounded memory under a
server pause.

### D5: Reconciliation is "snap + replay"

When the server's authoritative snapshot for tick T arrives:

1. The client calls `ackUpTo(T)` — discarding confirmed entries.
2. It compares the server transform at T with `get(T)->predictedTransform`.
3. If they differ beyond a small threshold, the client **snaps** to the
   authoritative state and calls `getPending(T)` to get the list of unconfirmed
   inputs.
4. It **replays** those inputs forward from the authoritative state, updating
   `predictedTransform` entries in-place.

This is the Gabriel Gambetta / Source Engine / Quake reconciliation pattern.
The visual artifact is a brief "snap" of the local player when the server
disagrees; at typical RTTs (< 150 ms) the snap is imperceptible.

Alternative: interpolate to authoritative state over N frames ("smooth
correction"). Better aesthetics; harder to implement and verify. Deferred to
Phase 5.5 as a quality-of-life improvement.

---

## Wire format summary

### InputMessage

```
input_message :=
    varint  tick           // client simulation tick
    bool    moveUp         // 1 bit each, packed by BitWriter
    bool    moveDown
    bool    moveLeft
    bool    moveRight
    bool    attack
    bool    dodge
```

Tick 0 encodes as a single byte (varint); at tick 2^21 it spills to 4 bytes.
Total size: 1–4 bytes (tick varint) + 6 bits (bools) → 2–5 bytes typical.

---

## Consequences

- `InputMessage` is usable by any game that has a `game::PlayerInput`-equivalent.
  The only coupling to the game layer is the six bool fields.
- `SnapshotBuffer` is rendering-backend-agnostic: it hands back interpolated
  `game::Transform` structs. The renderer does whatever it wants with them.
- `PredictionBuffer::getPending` returns a `std::vector` (copies). If this
  becomes a hot path under profiling it can return a span into the internal
  deque instead. For now clarity beats micro-optimisation.
- Phase 5b (the full `ReplicationServer` + `ReplicationClient` manager classes)
  will compose these three primitives. Nothing built here needs to change.
