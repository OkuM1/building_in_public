# 0012 — Measurement beats opinion

- **Date:** 2026-04-19
- **Commit:** *(this one, Commit 5 of Phase 3)*
- **Related:** [design/0012](../design/0012-benchmarks.md)
- **Principles:** Show numbers, write down the miss, pick the right target to chase

---

## What I built

A `bench/` folder with one microbenchmark (`bench_snapshot.cpp`),
wired into CMake behind `-DBUILD_BENCH=ON`, linking the `simulation`
target only. The benchmark measures full-snapshot encode+decode at
10/100/1000 entities and delta encode across four change rates. A
committed `bench/results.md` records numbers with their repro
command.

## Why (the problem)

Up to this point Phase 3 was working. Tests were passing. The
question I couldn't answer was **"is it good enough?"**. Good enough
is a number, and numbers come from benchmarks.

This is the commit where I had to confront the reality of what I
built instead of trusting my architectural intuitions about it.

## How I approached it

Two instincts pulled opposite directions.

The first was **"build the harness and commit whatever numbers you
get."** That's the honest move. The roadmap says "success looks like
numbers, not vibes" in its own words; if the number misses the
target, that's a finding, not a defeat.

The second was **"delay committing results until I've tuned them."**
Tuning first looks better in isolation but it's a trap: it confuses
"the system performs X" with "the benchmark reports X". Every hour
spent tuning before the first measurement is an hour spent in a
feedback-loop-less vacuum.

Picked the first. Ran the benchmark. Results:

- **Delta at 1 % change**: 82 bytes. Target 200. Met with 2.5× headroom.
- **Full snapshot at 1 k entities**: 15.9 KB. Target 4. Missed by 4×.

## The honest answer about the miss

The full-snapshot miss is real. Spent a while staring at it before
writing anything up. Three questions I asked myself before deciding
what to do about it:

1. **Is the target right?** Yes. 4 KB fits into a single typical MTU
   and is roughly what Quake 3 managed for a comparable entity count
   with tighter encoding. Not arbitrary.
2. **Is the measurement fair?** Yes. Release build, 1 000 entities,
   Transform+Velocity like our components actually ship. Not cheating.
3. **Does this number matter right now?** No. In steady-state,
   clients don't receive full snapshots — they receive deltas, which
   hit the target. Full snapshots are a *first-join* cost. The right
   follow-up is "log a Phase 3.5 optimisation item, name the specific
   bits leaking cost, and move on to Phase 4 where the measurement
   that matters lives."

Writing that chain of reasoning into the design note and `results.md`
felt more valuable than shaving bytes right now. Future-me reading
"delta target met, full-snapshot target missed, here's why we
de-prioritised it" will know *why* we moved on; if I'd silently
optimised, future-me wouldn't.

## Principle(s) this demonstrates

- **Ship the measurement, then decide.** Benchmarks are feedback, not
  grades. Running them before tuning reveals which axes actually need
  attention and which don't. Tuning first optimises for the wrong
  axis with high probability.
- **Numbers make disagreement cheap.** "The full snapshot is too big"
  and "the full snapshot is fine" are opinions without numbers.
  "15.9 KB, target 4 KB, miss is 4×, impact is first-join latency,
  steady-state is fine" is a position a future-me can agree with or
  push back on.
- **A documented miss is more valuable than a hidden one.** The miss
  is now in a committed file with a date, a repro command, and a
  cost decomposition. If this bites someone on a future client-join
  latency complaint, they have a map.
- **Pick the target that matches the hot path.** Full snapshots fire
  once per connection. Deltas fire 20 times per second per client
  forever. Optimising a 1/N cost when the N·∞ cost is healthy is
  almost always wrong.

## What I got wrong first

- My first thought when I saw the full-snapshot number was "quantise
  velocity right now and commit." That would have mixed a perf fix
  into a benchmark commit, breaking the "one concern per commit"
  discipline. Resisted; noted it in `results.md` instead.
- First version of the benchmark used `state.range(0)` for both entity
  count AND change percent, collapsing the change-rate sweep into
  the entity-count sweep. Numbers came out meaningless. Split into
  two benchmarks with separate `Arg()` sets. Clearer, more honest.
- I initially did not measure decode. "Who cares about decode?" —
  the client does, at 60 Hz, on a user's laptop. Added it. Good
  thing: decode is faster than encode (interesting and useful
  knowledge).

## Takeaway for future me

- Whenever a phase has a numeric target, the last commit of that
  phase is the one that measures it. Not a blog post later, not a
  vague "we'll benchmark in CI later" — a commit with `results.md`.
- When a target misses, don't hide it. Write a paragraph on (a) is
  the target right, (b) is the measurement fair, (c) does it matter
  now. Publish all three answers. That's the difference between a
  project that can be reasoned about later and one that can't.
- Release-mode microbenchmarks. Never publish Debug-mode numbers.
  It's a different codebase.
