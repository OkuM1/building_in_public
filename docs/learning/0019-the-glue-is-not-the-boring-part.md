# 0019 — The glue is not the boring part

- **Phase:** 4 closeout
- **Date:** 2026-05-07
- **Design:** [0019-connection-glue](../design/0019-connection-glue.md)

---

Each of the six Phase 4 sub-commits (socket, ack/RTT, channels,
fragmentation, congestion, network simulator) was designed and tested in
isolation. The design docs said "plumbing comes in the closeout commit."
Isolation made each sub-commit small and testable. The glue commit, by
contrast, is small *because* of all that isolation — but it is not trivial.

## The silence of `onPacketLost`

`ReliableChannel::onPacketLost` existed as a public method from Commit 4c.
Nothing called it. The design doc called this out as an explicit follow-up.
Writing the `Connection` glue immediately surfaced *where* it should be
called: after `processInboundHeader` returns the set of sequence numbers
the peer has irrecoverably moved past.

The lesson: a method that nobody calls is a promise to your future self.
It is only fulfilled when the surrounding machinery exists to invoke it.
Designing the method before its caller is still the right move — it keeps
the component's contract clear at definition time — but the gap between
"it exists" and "it works" can hide bugs for months if you don't close it.

## Loss detection as a scan, not a callback

The loss detection scan (walk 64 slots beyond the peer's ack horizon,
declare anything un-acked as lost) is bounded O(64) per received packet.
The alternative — a timer-per-message or an RTO system — requires a
monotonic clock threaded through every message and adds stateful edge
cases around RTT measurement. The scan approach falls out of the existing
`SequenceBuffer` without any new state: once a sequence slot is outside
the ack window and still unacknowledged, it is lost by definition.

The key insight: **you already have all the information you need.** The
peer's ack field tells you their horizon. The sent buffer tells you what
you sent. The gap between them is the loss set. No timers required.

## The milestone is the integration test

Phase 4 had a concrete milestone: two endpoints over 150 ms / 5 % loss,
stable. The individual sub-commit tests each validated one behaviour in
isolation. The milestone test validates the *composition*: that acks flow
back, loss declarations fire, retransmits go out, and the congestion
controller tracks RTT — all in the right order, on every tick.

The milestone test passed on the first run. That is not luck. It is the
consequence of each primitive being tested independently before they were
assembled. Integration tests rarely fail if the units are solid.

## What the glue reveals about Phase 5

`Connection` has a clean per-peer API: `send`, `buildPacket`,
`receivePacket`, `receive`. The server's per-tick loop will call exactly
these methods for each connected client. Phase 5 (replication model) sits
entirely above this surface — it never touches sockets, headers, ack bits,
or sequence numbers. The layering worked.
