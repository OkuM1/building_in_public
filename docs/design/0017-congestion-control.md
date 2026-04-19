# ADR 0017 — Congestion Control: Gaffer's Good Mode / Bad Mode

Status: accepted
Phase: 4e

## Context

Phase 4b gave us smoothed RTT from `ReliableEndpoint`. A blind 60 Hz
send loop ignores that signal: on a pipe that is already hurting,
we make things worse.

Real congestion-control algorithms (BBR, Cubic, LEDBAT) are tuned
for TCP-shaped bulk transfers. Our traffic is small regular packets
with latency budgets measured in tens of milliseconds. We want a
controller that:

1. **Backs off fast** when the link degrades (one sample over
   threshold is enough).
2. **Recovers conservatively** so a flappy link can't whip our send
   rate back and forth.
3. **Is auditable** — every knob is a named constant a human can
   reason about.

## Decision

Implement Gaffer's two-state controller (`CongestionMode::Good`,
`CongestionMode::Bad`) with a hysteresis timer ("penalty") that
doubles on every consecutive flap.

- **GOOD -> BAD**: single RTT sample > `rttBadThresholdMs`. Immediate.
- **BAD -> GOOD**: penalty since BAD entry must have elapsed AND
  RTT must have stayed under threshold for `goodUpgradeAfterMs`.
- **Flap penalty**: `initialPenaltyMs` on the first drop; doubles on
  each consecutive flap; capped at `maxPenaltyMs`.
- **Reset**: after `penaltyResetAfterMs` of sustained GOOD, the
  penalty resets to `initialPenaltyMs`. A well-behaved link gets a
  clean slate.

Output is a single scalar: `sendRateHz()` — 30 Hz in GOOD, 10 Hz in
BAD by default. Callers convert it into a per-tick gate.

## What this controller does NOT do

- **No per-packet size budget.** Size control is the MTU / fragmenter's
  job (Phase 4d). Rate and size are independent levers.
- **No socket ownership.** Pure state machine, caller supplies RTT
  and a clock reading. Matches the injected-`Clock` pattern from
  ADR 0014.
- **No loss-rate input.** Phase 4b's RTT is the single signal. Loss
  rate as a secondary trigger is an explicit follow-up.

## Alternatives considered

- **AIMD (TCP-style).** Tuned for bulk transfers; reacts to loss
  rather than latency. Overkill and wrong-shaped.
- **Per-ack token bucket.** Flexible but harder to reason about; the
  two-state machine covers our use case with four knobs.
- **Three modes (GOOD / OK / BAD).** Considered and rejected — the
  extra state doubles the transition table without a concrete
  scenario where BAD alone isn't enough.

## Follow-ups

- Wire `millisBetweenSends()` into a per-peer send pump once the
  socket+endpoint facade is built (end of Phase 4).
- Consider a loss-rate secondary trigger once Phase 4f's network
  simulator lets us reproduce lossy-but-low-RTT conditions.
