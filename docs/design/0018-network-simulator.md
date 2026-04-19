# ADR 0018 — Network Simulator: In-Process Lossy Link

Status: accepted
Phase: 4f

## Context

The ROADMAP Phase 4 closeout milestone is: *two processes exchange
reliable + unreliable messages over loopback with injected 150 ms /
5 % loss and remain stable.* Getting to that milestone requires a
way to reproduce those conditions deterministically. Real
`netem`/`tc` rules work on Linux but:

1. Require root and break CI.
2. Are not deterministic (kernel scheduler timing).
3. Don't let a test step *time* forward — they're wall-clock bound.

We need an **in-process** packet simulator so:

- Unit tests can drive arbitrary latency/jitter/loss profiles.
- Integration tests can step a `FakeClock` through a minute of
  simulated network activity in milliseconds of wall time.
- Reproducing a bug from a CI log requires only re-using a seed.

## Decision

`SimulatedLink` — a pure data structure. `send(bytes, now)` enqueues
with a delivery time of `now + latency + jitter`; `receive(now)`
drains all packets whose delivery time has passed, in delivery-time
order (ties broken by send sequence). Loss is a per-packet Bernoulli
trial. RNG is a seeded `mt19937_64`.

- Reordering falls out of jitter for free. No separate reorder knob.
- Delivery order is priority-queue by `(deliverAt, sequence)`.
- Both `jitter` and `drop` rolls are drawn on every `send()`, even
  when the corresponding knob is zero, so the RNG stream is stable
  across parameter sweeps.

## What this class does NOT do

- **No bandwidth cap.** That's a sender-side concern owned by
  Phase 4e's `CongestionController`.
- **No duplication.** Real UDP duplicates are rare and channels
  already dedupe. Defer until there's a concrete need.
- **No MTU enforcement.** Sender-side (Phase 4d `Fragmentation`).

## Alternatives considered

- **Wrap `UdpSocket` with a proxy thread.** Real sockets, real
  timing, real non-determinism. Good for the final soak test,
  useless in unit tests.
- **Separate reorder probability.** Double-counting what jitter
  already models. Rejected.
- **Per-direction configs on a `Link` pair.** Trivial to layer on
  top later by holding two `SimulatedLink` instances; no need to
  bake asymmetry into the primitive.

## Follow-ups

- Phase 4 closeout glue commit: two `ReliableEndpoint`+channel
  stacks on opposite ends of two `SimulatedLink`s, drive with
  `FakeClock`, assert stability at 150 ms / 5 % over N seconds.
- Loss-burst model (Gilbert-Elliott) if simple Bernoulli misses a
  real-world scenario.
- Plug the simulator in behind `UdpSocket`'s `send`/`recv` via a
  small abstraction for end-to-end integration without a real
  socket. Only if needed.
