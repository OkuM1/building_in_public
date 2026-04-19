# Phase 3 benchmark results

> Measured on: 2026-04-19, Linux, 24× 3.7 GHz CPU, GCC Release `-O2`.
>
> Re-run: `cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCH=ON -DBUILD_TESTS=OFF && cmake --build build-bench --target bench_snapshot && ./build-bench/bench_snapshot`
>
> These are single-machine micro-benchmarks. They bound the serialisation
> cost in isolation; Phase 4 will measure the full client → server → client
> loop under simulated latency/loss.

---

## Full snapshot: encode

| Entities | Time      | Size (bytes) | Bytes / entity | Target met? |
|---------:|----------:|-------------:|---------------:|:-----------:|
| 10       | 302 µs    | 152          | 15.2           | —           |
| 100      | 297 µs    | 1 502        | 15.0           | —           |
| **1000** | **372 µs**| **15 875**   | **15.9**       | ❌ (target < 4 KB) |

The roadmap target of **< 4 KB full snapshot at 1000 entities** is
**NOT** met by the v1 format. At ~16 B/entity the budget is blown by
4×. The per-entity cost decomposes roughly as:

| Contribution                         | Bytes |
|--------------------------------------|------:|
| Transform (16+16+8 bits, quantised)  | 5     |
| Velocity (32+32 bits, **raw float**) | 8     |
| Component mask (fixed)               | 1     |
| Entity id (varint, typically 1–2)    | 1–2   |
| **Total**                            | **~15–16** |

Closing the gap is a mix of:

1. **Quantise Velocity** — replace 64-bit raw floats with 16-bit
   quantised per axis once the Sumo Arena velocity range is pinned.
   Saves ~6 B/entity → ~10 B/entity → **~10 KB at 1000 entities**
   (still over, but halved).
2. **Delta-coded entity ids** — emit `id - prev_id` instead of the
   absolute id. Most ticks assign sequential ids, so 90%+ would be
   1-byte varints guaranteed. Marginal at 1 B/entity, but compounding.
3. **Per-snapshot schema (no per-entity mask).** The mask byte is
   paying for flexibility most snapshots don't need. A single
   "schema" byte at the snapshot header plus a mask-once-per-archetype
   pattern saves 1 B/entity at the cost of losing per-entity
   component variation. Revisit if we want it.

Decision: **don't chase the full-snapshot target this phase.** In
practice only brand-new clients receive a full snapshot; steady-state
bandwidth is dominated by deltas, and the delta target IS met (see
below). Full-snapshot optimisation is parked as a Phase 3.5 item if
client-join latency ever shows up as a problem.

---

## Full snapshot: decode

| Entities | Time     | Throughput |
|---------:|---------:|-----------:|
| 10       | 28 µs    | 5.1 MiB/s  |
| 100      | 46 µs    | 30.9 MiB/s |
| 1000     | 183 µs   | 82.9 MiB/s |

Faster than encode at 1 000 entities (183 µs vs 372 µs) because the
encode path does two passes over `MAX_ENTITIES` (count, then emit).
Not a concern — at 60 Hz this is <1% of a frame even per-client.

---

## Delta: encode at N=1000, change-rate sweep

| Changes | Time   | Size (B)  | vs full snapshot | Target met? (200 B) |
|--------:|-------:|----------:|-----------------:|:-------------------:|
| 0 %     | 1 033 µs | 12      | 0.08 %  | ✅ |
| **1 %** | **1 003 µs** | **82** | **0.52 %** | ✅ (< 200 B) |
| 10 %    | 1 006 µs | 712     | 4.48 %  | ❌ (busy tick) |
| 100 %   | 1 030 µs | 7 877    | 49.6 %  | — (worst case) |

**Steady-state (1 % of entities moved/tick) = 82 bytes. Target was
< 200 B. Met with 2.5× headroom.**

Even a "busy tick" at 10 % change is 712 bytes — well under the
32 KB/s downstream budget at 20 Hz snapshot rate (=1.6 KB per
snapshot budget / client). The 100 % case is still half the size of
a full snapshot thanks to the saved entity-id overhead not being
present for unchanged entities.

### Encode time

Delta encode is ~3× slower than full snapshot (1.0 ms vs 0.37 ms at
1 000 entities). Reason: the v1 encoder walks `MAX_ENTITIES = 10 000`
*three times* — count changed, emit changed, count+emit removed —
with a per-slot `hasComponent<T>` probe each pass. At 20 Hz that's
2 % of a single core per client; comfortable today.

**Optimisation on deck** if the server ever pushes 100+ clients:
cache the set of live replicated entities in `World` and iterate it
instead of `MAX_ENTITIES`. Estimated speedup: 10×.

---

## Summary against the roadmap

| Roadmap target                          | Result    | Status |
|-----------------------------------------|-----------|:------:|
| Full snapshot @ 1k entities < 4 KB      | 15.9 KB   | ❌ |
| Typical delta snapshot < 200 B          | 82 B      | ✅ |
| Server tick 60 Hz / 16 clients headroom | not yet measured (Phase 4) | — |
| Downstream < 32 KB/s @ 20 Hz            | 1.64 KB/snapshot at 1 % change ⇒ 32.8 KB/s budget ≈ 5 % used | ✅ (model) |

The full-snapshot miss is logged as a Phase 3.5 optimisation item.
Delta encoding carries the bandwidth load and comfortably hits target.
