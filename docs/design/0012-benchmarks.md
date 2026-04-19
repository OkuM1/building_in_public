# 0012 — Phase 3 benchmark wiring + published numbers

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 3 (Serialization & snapshots) — Commit 5/5
- **Related:** [0008](0008-bitstream.md), [0010](0010-snapshot-format.md), [0011](0011-delta-encoding.md)

## Context

The roadmap puts two numeric targets on Phase 3: full snapshot < 4 KB
at 1 k entities, typical delta < 200 B. Without measurement those are
hopes. Commit 5 adds google-benchmark, writes a snapshot/delta
micro-benchmark, and commits the results to [`bench/results.md`](../../bench/results.md).

## Decision

### Add `bench/` with `google-benchmark` (opt-in)

- CMake option `BUILD_BENCH` defaults OFF. When ON, `FetchContent`
  pulls google-benchmark v1.8.3 and adds a `bench_snapshot` target.
- Benchmarks link the `simulation` target only — same headless
  discipline as the server and unit tests. The CMake GL-free
  assertion from commit 0006 still holds.
- Release-mode only by convention. Debug-mode microbenchmarks are
  noise.

### Measure four axes

1. **Encode throughput** at 10 / 100 / 1 000 entities.
2. **Decode throughput** at the same points.
3. **Encode size** (reported via `state.counters["bytes"]`).
4. **Delta-encode size + time** as the change-rate sweeps 0 / 1 / 10
   / 100 %.

### Publish numbers in-repo, not just in CI

`bench/results.md` is committed, dated, and reproducible via the
command in its header. This is the same discipline the roadmap's
"success looks like numbers" line calls for.

## Alternatives considered

1. **Catch2 `BENCHMARK` blocks.** Lighter weight, but Catch's stats
   model is less rigorous than google-benchmark's, and we'd be
   mixing a test runner with a perf runner. Rejected.
2. **Criterion.rs-style JSON output + a CI perf gate.** Desirable
   long-term; premature today. Park for Phase 4 or 5.
3. **Run benchmarks as part of `ctest`.** Tempting for uniformity
   but drags noise into every test run and Debug-mode timing is
   nonsense. Rejected.

## Consequences

- **Result:** delta target **met** (82 B at 1 % change vs 200 B
  budget); full-snapshot target **missed** (15.9 KB vs 4 KB). Full
  discussion and cost decomposition in `bench/results.md`.
- **Decision:** the full-snapshot miss is parked as a Phase 3.5
  optimisation item. The steady-state bandwidth path is deltas, and
  deltas carry 5 % of the downstream budget at 1 000 entities. Full
  snapshots only run on new-client join; optimising them is a
  latency-on-join concern, not a bandwidth one.
- **Follow-ups** documented in `results.md`:
  - Quantise `Velocity` once physics pin a range (saves ~6 B/entity).
  - Delta-code entity ids (saves ~1 B/entity).
  - Cache the live-replicated entity set to make delta encoding 10×
    faster when client count grows.

## Code pointers

- [bench/bench_snapshot.cpp](../../bench/bench_snapshot.cpp) — the
  benchmark harness.
- [bench/results.md](../../bench/results.md) — committed numbers.
- [CMakeLists.txt](../../CMakeLists.txt) — `BUILD_BENCH` option and
  `bench_snapshot` target.
