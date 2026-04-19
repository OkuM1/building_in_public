# 0018 — Reproducibility is a Feature

Phase 4f was the shortest feature in the whole engine so far. An
in-process packet simulator is a priority queue, a dice roll, and a
clock. That's it. But the reason it exists matters more than the
code does.

Every earlier networking primitive in this codebase — sockets, ack
bitfields, channels, fragmentation, congestion control — was built
against adversarial unit tests: shuffled fragments, dropped middle
packets, flapping RTT. Each of those tests was hand-rolled against
*that specific primitive*. `SimulatedLink` is the point where I get
to stop hand-rolling and let the full stack be exercised end-to-end
against a link that *actually behaves like a network*: latency,
jitter, loss, reordering — all controllable, all reproducible.

The one design decision I nearly got wrong: I was about to draw
from the RNG conditionally (`if jitter > 0 then draw`). That means
turning jitter on mid-test changes the loss roll of every
subsequent packet, because the dice are now stepping through the
sequence at a different rate. So I draw both rolls on every `send`,
unconditionally, even when the knob is zero. Knobs are knobs; the
dice are the dice. Keeping them independent means a test that
sweeps `lossProbability` from 0 % to 20 % gets *the same jitter
profile* across the sweep, and a bug that shows up at 15 % loss
points at the loss model, not at a coincidental reorder.

Lesson: reproducibility isn't "seed the RNG and go". It's
"structure your code so the same logical event draws the same
number of samples regardless of configuration". The seed is only
half of it.
