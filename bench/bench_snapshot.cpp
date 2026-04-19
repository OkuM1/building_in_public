/// @file bench_snapshot.cpp
/// @brief Micro-benchmarks for the Phase 3 snapshot + delta encoders.
///
/// Goals:
///   - Measure snapshot size at 10 / 100 / 1000 entities against the
///     roadmap target of < 4 KB at 1000 entities.
///   - Measure delta size at several change rates (0% / 1% / 10% / 100%)
///     against the target "typical delta < 200 B".
///   - Measure encode / decode throughput (ns per operation) so we can
///     spot regressions in future commits.
///
/// Each benchmark uses a fresh `engine::World`, populates it with N
/// entities, runs the encoder in a hot loop, and uses
/// `SetBytesProcessed` so the output includes MB/s.
///
/// @see docs/design/0012-benchmarks.md

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>

#include "engine/ecs/World.h"
#include "engine/net/BitStream.h"
#include "engine/net/Serialize.h"
#include "game/components/ComponentSerializers.h"
#include "game/components/GameComponents.h"

using engine::EntityId;
using engine::World;
using engine::net::BitReader;
using engine::net::BitWriter;
using engine::net::GameReplication;

namespace {

/// Fresh world with the three replicated components registered.
World makeWorld() {
    World w;
    w.registerComponent<game::Transform>();
    w.registerComponent<game::Velocity>();
    w.registerComponent<game::PlayerInput>();
    return w;
}

/// Populate `w` with `n` entities, each with a Transform and Velocity
/// at deterministic-but-varied positions (so quantisation is exercised).
void populate(World& w, int n, uint32_t seed = 0xC0FFEEu) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> velDist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> rotDist(0.0f, 6.2831853f);

    for (int i = 0; i < n; ++i) {
        const EntityId e = w.createEntity();
        w.addComponent<game::Transform>(e,
            {posDist(rng), posDist(rng), rotDist(rng)});
        w.addComponent<game::Velocity>(e,
            {velDist(rng), velDist(rng)});
    }
}

/// Clone `src` via a full snapshot round-trip. Produces a world whose
/// replicated state equals `src` within quantiser bounds — exactly
/// what a client would hold after applying a baseline snapshot.
World cloneViaSnapshot(World& src) {
    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, 0, out);
    const auto& bytes = out.finish();

    World dst = makeWorld();
    // Burn ids to match src's allocations (src used 0..n-1 in order).
    // We compute how many entities src has by counting non-empty masks.
    for (EntityId e = 0; e < engine::MAX_ENTITIES; ++e) {
        const uint8_t m = engine::net::detail::computeMaskImpl(
            src, e, GameReplication{},
            std::make_index_sequence<GameReplication::count>{});
        if (m != 0) (void)dst.createEntity();
    }

    BitReader in(bytes.data(), bytes.size());
    uint32_t t = 0;
    engine::net::decodeSnapshot<GameReplication>(in, dst, t);
    return dst;
}

}  // namespace

// ============================================================================
// Full snapshots
// ============================================================================

static void BM_EncodeSnapshot(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    World w = makeWorld();
    populate(w, n);

    std::size_t lastSize = 0;
    for (auto _ : state) {
        BitWriter out;
        engine::net::encodeSnapshot<GameReplication>(w, 0, out);
        const auto& bytes = out.finish();
        lastSize = bytes.size();
        benchmark::DoNotOptimize(bytes.data());
    }
    state.counters["bytes"] = static_cast<double>(lastSize);
    state.counters["bytes_per_entity"] =
        n > 0 ? static_cast<double>(lastSize) / n : 0.0;
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(lastSize));
}
BENCHMARK(BM_EncodeSnapshot)->Arg(10)->Arg(100)->Arg(1000);

static void BM_DecodeSnapshot(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    World src = makeWorld();
    populate(src, n);

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, 0, out);
    const auto bytes = out.finish();  // copy so finish() isn't re-called

    for (auto _ : state) {
        // Fresh world each iter so decode does real work (adding components).
        state.PauseTiming();
        World dst = makeWorld();
        for (int i = 0; i < n; ++i) (void)dst.createEntity();
        state.ResumeTiming();

        BitReader in(bytes.data(), bytes.size());
        uint32_t tick = 0;
        engine::net::decodeSnapshot<GameReplication>(in, dst, tick);
        benchmark::DoNotOptimize(tick);
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(bytes.size()));
}
BENCHMARK(BM_DecodeSnapshot)->Arg(10)->Arg(100)->Arg(1000);

// ============================================================================
// Deltas — change rate sweep at N=1000
// ============================================================================

/// Encode a delta where `changePercent` percent of entities had their
/// Transform.x nudged since baseline. The baseline and current worlds
/// are otherwise equal.
static void BM_EncodeDelta(benchmark::State& state) {
    const int n = 1000;
    const int changePercent = static_cast<int>(state.range(0));

    World baseline = makeWorld();
    populate(baseline, n);
    World current = cloneViaSnapshot(baseline);

    // Apply the desired change rate to `current`.
    const int changes = n * changePercent / 100;
    for (int i = 0; i < changes; ++i) {
        const EntityId e = static_cast<EntityId>(i);
        // Nudge by much more than quantiser step so equal() returns false.
        current.getComponent<game::Transform>(e).x += 0.1f;
    }

    std::size_t lastSize = 0;
    for (auto _ : state) {
        BitWriter out;
        engine::net::encodeDelta<GameReplication>(baseline, current, 1, 0, out);
        const auto& bytes = out.finish();
        lastSize = bytes.size();
        benchmark::DoNotOptimize(bytes.data());
    }
    state.counters["bytes"] = static_cast<double>(lastSize);
    state.counters["pct_of_full"] = static_cast<double>(lastSize);
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(lastSize));
}
BENCHMARK(BM_EncodeDelta)
    ->Arg(0)    // no changes -> minimal header
    ->Arg(1)    // 1% — "typical" tick
    ->Arg(10)   // 10% — busy tick
    ->Arg(100); // everything moved — worst case

BENCHMARK_MAIN();
