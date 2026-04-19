/// @file test_delta.cpp
/// @brief Unit tests for `encodeDelta` / `applyDelta`.
///
/// Invariants tested:
///   1. No changes → tiny delta (just 3 header varints).
///   2. Single-field change → only that entity, only that component bit set.
///   3. Adding a component → it appears in the delta even if the entity existed.
///   4. Removing a component (i.e., removing the whole entity's replicated
///      set) → entry in the removed list.
///   5. Apply(delta) on a copy of baseline → equals current within
///      quantiser bounds.
///   6. Sub-quantum float drift (Transform) does NOT produce a delta
///      entry: that's the whole point of the `equal` trait method.

#include <doctest/doctest.h>

#include <cmath>

#include "engine/ecs/World.h"
#include "engine/net/BitStream.h"
#include "engine/net/Quantize.h"
#include "engine/net/Serialize.h"
#include "game/components/ComponentSerializers.h"
#include "game/components/GameComponents.h"

using engine::EntityId;
using engine::World;
using engine::net::BitReader;
using engine::net::BitWriter;
using engine::net::GameReplication;

namespace {

World makeWorld() {
    World w;
    w.registerComponent<game::Transform>();
    w.registerComponent<game::Velocity>();
    w.registerComponent<game::PlayerInput>();
    return w;
}

// Apply a full snapshot-style clone: encode current, decode into a
// fresh world. Used to produce a "baseline" that's bit-equal to what
// a receiver would hold after the previous full snapshot.
World cloneViaSnapshot(World& src) {
    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, /*tick=*/0, out);
    const auto& bytes = out.finish();

    World dst = makeWorld();
    // Pre-create entities so their ids match. `createEntity` returns
    // ids in order 0, 1, 2, ...; we assume src allocated the same way.
    for (EntityId e = 0; e < engine::MAX_ENTITIES; ++e) {
        const uint8_t mask =
            engine::net::detail::computeMaskImpl(
                src, e, GameReplication{},
                std::make_index_sequence<GameReplication::count>{});
        if (mask != 0) {
            while (dst.createEntity() < e) { /* burn ids to reach e */ }
        }
    }

    BitReader in(bytes.data(), bytes.size());
    uint32_t t = 0;
    engine::net::decodeSnapshot<GameReplication>(in, dst, t);
    return dst;
}

}  // namespace

TEST_CASE("Delta: identical worlds produce a minimal delta") {
    World a = makeWorld();
    const EntityId e = a.createEntity();
    a.addComponent<game::Transform>(e, {1.0f, 2.0f, 0.5f});

    World b = cloneViaSnapshot(a);

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(b, a, /*tick=*/10, /*baseline=*/9, out);
    const auto& bytes = out.finish();

    // Header: tick(1) + baseline_tick(1) + changed_count(1) + removed_count(1)
    // = 4 bytes. No entity entries.
    CHECK(bytes.size() == 4u);
}

TEST_CASE("Delta: single Transform change produces single-component delta") {
    World baseline = makeWorld();
    const EntityId e = baseline.createEntity();
    baseline.addComponent<game::Transform>(e, {0.0f, 0.0f, 0.0f});
    baseline.addComponent<game::Velocity>(e, {0.5f, 0.25f});

    // Current = baseline + moved position; velocity unchanged.
    World current = cloneViaSnapshot(baseline);
    current.getComponent<game::Transform>(e).x = 1.0f;

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(baseline, current,
                                              /*tick=*/20, /*baseline=*/19, out);
    const auto& bytes = out.finish();

    // Layout:
    //   tick varint           1 byte
    //   baseline varint       1 byte
    //   changed_count varint  1 byte
    //     entity id varint    1 byte
    //     mask byte           1 byte   (0b001 = Transform only)
    //     Transform payload   5 bytes
    //   removed_count varint  1 byte
    // Total: 11 bytes.
    CHECK(bytes.size() == 11u);
    // Entry layout position of the mask byte: index 4.
    CHECK(bytes[4] == 0b001);
}

TEST_CASE("Delta: adding a new entity appears as a changed entry with full mask") {
    World baseline = makeWorld();
    const EntityId first = baseline.createEntity();
    baseline.addComponent<game::Transform>(first, {0.0f, 0.0f, 0.0f});

    World current = cloneViaSnapshot(baseline);
    const EntityId e = current.createEntity();
    current.addComponent<game::Transform>(e, {1.0f, 2.0f, 0.0f});
    current.addComponent<game::Velocity>(e, {3.0f, 4.0f});

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(baseline, current, 1, 0, out);
    const auto& bytes = out.finish();

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0, baselineTick = 0;
    REQUIRE(engine::net::applyDelta<GameReplication>(in, baseline, tick, baselineTick));

    // Baseline now has Transform + Velocity for `e`.
    CHECK(baseline.hasComponent<game::Transform>(e));
    CHECK(baseline.hasComponent<game::Velocity>(e));
    const float pStep = (2.0f * engine::net::kWorldExtent) /
                        static_cast<float>((1u << engine::net::kPositionBits) - 1u);
    CHECK(std::fabs(baseline.getComponent<game::Transform>(e).x - 1.0f) <= pStep);
}

TEST_CASE("Delta: removing an entity appears in the removed list") {
    World baseline = makeWorld();
    const EntityId keep = baseline.createEntity();
    const EntityId drop = baseline.createEntity();
    baseline.addComponent<game::Transform>(keep, {0.0f, 0.0f, 0.0f});
    baseline.addComponent<game::Transform>(drop, {1.0f, 1.0f, 0.0f});

    World current = cloneViaSnapshot(baseline);
    current.destroyEntity(drop);

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(baseline, current, 1, 0, out);
    const auto& bytes = out.finish();

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0, baselineTick = 0;
    REQUIRE(engine::net::applyDelta<GameReplication>(in, baseline, tick, baselineTick));

    CHECK(baseline.hasComponent<game::Transform>(keep));
    CHECK_FALSE(baseline.hasComponent<game::Transform>(drop));
}

TEST_CASE("Delta: applying a delta makes baseline match current (within quantiser)") {
    World baseline = makeWorld();
    const EntityId a = baseline.createEntity();
    const EntityId b = baseline.createEntity();
    baseline.addComponent<game::Transform>(a, {0.0f, 0.0f, 0.0f});
    baseline.addComponent<game::Velocity>(a, {0.0f, 0.0f});
    baseline.addComponent<game::Transform>(b, {2.0f, -1.0f, 1.5f});

    World current = cloneViaSnapshot(baseline);
    // Many changes: move `a`, retarget `a`'s velocity, drop `b`'s rotation
    // change (quantised), add PlayerInput to `a`.
    current.getComponent<game::Transform>(a) = {0.7f, -0.3f, 2.1f};
    current.getComponent<game::Velocity>(a) = {5.0f, 0.0f};
    game::PlayerInput pi{};
    pi.moveRight = true;
    current.addComponent<game::PlayerInput>(a, pi);

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(baseline, current, 99, 42, out);
    const auto& bytes = out.finish();

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0, baselineTick = 0;
    REQUIRE(engine::net::applyDelta<GameReplication>(in, baseline, tick, baselineTick));
    CHECK(tick == 99u);
    CHECK(baselineTick == 42u);

    const float pStep = (2.0f * engine::net::kWorldExtent) /
                        static_cast<float>((1u << engine::net::kPositionBits) - 1u);

    // baseline now matches current within quantiser bounds.
    const auto& ta = baseline.getComponent<game::Transform>(a);
    CHECK(std::fabs(ta.x - 0.7f)  <= pStep);
    CHECK(std::fabs(ta.y + 0.3f)  <= pStep);
    CHECK(baseline.getComponent<game::Velocity>(a).vx == doctest::Approx(5.0f));
    CHECK(baseline.hasComponent<game::PlayerInput>(a));
    CHECK(baseline.getComponent<game::PlayerInput>(a).moveRight == true);
}

TEST_CASE("Delta: sub-quantum float drift does NOT produce a delta entry") {
    World baseline = makeWorld();
    const EntityId e = baseline.createEntity();
    baseline.addComponent<game::Transform>(e, {1.0f, 1.0f, 0.0f});

    World current = cloneViaSnapshot(baseline);
    // Quantiser step at 16 bits over [-16, 16] is ~4.88e-4. Drift by
    // an amount *much* smaller than that: should round to the same
    // code and produce an empty delta.
    current.getComponent<game::Transform>(e).x = 1.0f + 1e-7f;
    current.getComponent<game::Transform>(e).y = 1.0f - 1e-7f;

    BitWriter out;
    engine::net::encodeDelta<GameReplication>(baseline, current, 1, 0, out);
    const auto& bytes = out.finish();

    // No entity entries. Just 4 header varints = 4 bytes.
    CHECK(bytes.size() == 4u);
}
