# 0015 — Three channels, one idea

- **Date:** 2026-04-19
- **Commit:** Phase 4c — channels
- **Related:** [design/0015](../design/0015-channels.md), [learning/0014](0014-ack-is-a-coordinate-system.md)
- **Principles:** Name the reliability at the type level, tag don't time, the contrast makes the concept

---

## What I built

Three message-delivery classes sitting above the packet-ack machinery:
`UnreliableChannel`, `ReliableChannel` (unordered by default,
ordered via ctor flag). Sender records which packet an outbound
message went in; when `onPacketAcked` fires, the matching messages
leave the outbox. Receiver dedupes by 16-bit message id, and in
ordered mode holds non-contiguous ids until the gap fills.

8 new tests. Tracks which packet carried which message id; ordered
delivery correctness; duplicate dedup; retransmit on declared loss.

## Why (the problem)

Phase 4b got me a signal: "packet N was received by the peer." Phase
4c is the commit that translates that low-level signal into the three
reliability *flavours* the engine actually wants:

- Snapshots don't care about drops; the next snapshot fixes it.
- "You were eliminated" cares very much about drops.
- "Round 2 begins now" cares about drops AND order.

I could have written one class and parameterised it three ways, or
three classes. Pick carefully — this type surface will be read a lot.

## How I approached it

### Why three classes, not one enum

Early in the design I had `class Channel { Reliability mode; ... };`.
It felt clean, but every public method grew a `switch (mode_)`
inside. The worst offender: `writeInto`. For an unreliable channel,
it doesn't take a `packetSeq` argument (there's no ack bookkeeping to
do); for a reliable one, it requires a `packetSeq` argument (that
tag is the whole mechanism). Putting both into one class means the
call site looks like

```cpp
unreliable.writeInto(w, /* packetSeq= */ 0);   // arg ignored
reliable.writeInto(w, /* packetSeq= */ current);
```

That's a type-system failure. The "arg ignored" branch is a
continuously renewable source of bugs. Split into two classes:
`UnreliableChannel::writeInto(w)` and
`ReliableChannel::writeInto(w, packetSeq)`. The compiler now enforces
the distinction, and anyone reading call sites can see it.

Reliable-ordered vs reliable-unordered *does* live in one class, via
a ctor flag, because they differ only on the receive side and
sharing the sender prevents a bug in one from drifting out of the
other.

**Principle I'm taking away from this: if two variants of a thing
have different signatures, they are two classes. If they have the
same signature but different behaviour, they can be one.**

### Why "tag the message" instead of "time the message"

The standard reliable-UDP pattern is per-message RTO timers: every
outbound message has a send timestamp; if `now - sent > RTO`,
retransmit. That's what TCP does.

I'm not TCP. I have an ack signal that already tells me per-packet
success/failure. If the sender tags each message with "what packet
did I last put you in," then:

- Ack flow removes matching messages.
- Loss flow (caller-declared, Phase 4 closeout) releases the tag for
  retransmit.
- Nothing needs a timer. Nothing needs SRTT. Nothing needs clock.

This collapses a lot of complexity. The price is that *loss
detection itself* must happen somewhere — but it does, once, in the
outer layer (the `ReliableEndpoint` receive-window walk) rather than
in each channel independently.

The temptation to add a per-message timer for robustness was real. I
resisted. If loss detection at the outer layer is ever late enough
that reliable messages stall, I'll add a belt-and-braces per-message
deadline. Not before.

### The contrast is the concept

Writing all three channels in the same file clarifies the concept in
a way that writing any one of them in isolation wouldn't have.

- `UnreliableChannel::writeInto` — drains the outbox.
- `ReliableChannel::writeInto` — skips messages tagged in-flight.
- `ReliableChannel (ordered)::readFrom` — reliable + holdback.

The deltas between them ARE the concept of "reliability." Reading
`Channel.h` side-by-side is genuinely more informative than reading a
monograph on netcode patterns. I'll keep doing this when a commit has
multiple closely-related variants: put them in one file, rely on the
contrast to teach.

## Principle(s) this demonstrates

- **If the signature differs, split the class.** Don't paper over
  with "arg ignored in mode X." Let the type system enforce what mode
  *means*.
- **Tag, don't time.** If you already have a signal that carries the
  information, hang your state off that instead of inventing a
  parallel one.
- **Small related variants belong in one file.** The contrast teaches
  the reader what the concept is.
- **Document what you chose NOT to do.** "Bounded dedup window" and
  "MTU enforcement" are known gaps, named in the ADR with rationale
  for not doing them here. Future-me reading this commit sees intent,
  not oversight.

## What I got wrong first

- First receive-side dedup was a `std::vector<uint16_t>` of delivered
  ids, linearly scanned. At 100 msgs/sec for 10 seconds that's
  already a 1000-element O(N) lookup per message. Switched to
  `unordered_map<uint16_t, bool>` before writing a single test.
  (Still not the ideal structure — see ADR's bounded-window
  follow-up — but O(1).)
- Ordered-mode receive initially delivered the holdback recursively.
  Replaced with a non-recursive `while (holdback_.find(expected))`
  drain loop; indistinguishable behaviour, but won't blow the stack
  if someone ever sends 50k messages out of order.
- Nearly wrote a test using `sleep_for(rtt_ms)`. Remembered 4b's
  lesson (learning note 0014) and kept all tests clock-free.
  Channels have no clock dependency — that's a property worth keeping.

## Takeaway for future me

- Don't tolerate signatures with "argument ignored in mode X." It's
  the type system telling you it's two things.
- When a new primitive could use its own timer OR hang state off an
  existing signal, use the signal. Less state, fewer race surfaces.
- Collocating close variants is underrated documentation.
- `onPacketLost` is a public API with no callers yet. That's fine.
  Phase 4 closeout wires it. Leaving it dangling is correct when the
  commit boundary doesn't include the wiring — better than inlining
  a bogus half-wired dependency.
