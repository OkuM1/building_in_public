# 0017 — Two Modes Beat a Curve

Building Phase 4e, I spent ten minutes sketching a continuous
controller: map RTT to send-rate via a smooth curve, throw in a
dampening term, done. Then I stopped.

A continuous controller has infinity knobs. Every one is a place to
bikeshed and a place a future me misconfigures at 2 AM. A two-state
machine with a hysteresis timer has four knobs, each one a named
constant with a clear story: "how fast when good", "how fast when
bad", "when do we decide it's bad", "how long do we stay scared".

The flappy-link case is the whole reason hysteresis exists. Without
it, a link bouncing around the threshold produces a send rate
oscillating between 10 and 30 Hz — worst of both worlds. With a
doubling penalty, a flappy link just burns itself into BAD and stays
there until it behaves for a sustained period. The controller
*refuses to oscillate*.

The test that taught me something was not the happy path. It was
the one where I tried to upgrade BAD->GOOD by issuing two good-RTT
samples one millisecond apart and failing to upgrade. The
stable-good timer needs real elapsed time before it fires — that's
the whole point. My test was trying to cheat hysteresis and the
controller correctly refused. Hysteresis is worth exactly as much
as the time you let it cook.

Lesson: when you catch yourself reaching for a continuous signal,
check whether two states and a clock will do. They almost always will,
and you'll have fewer 2-AM questions to answer.
