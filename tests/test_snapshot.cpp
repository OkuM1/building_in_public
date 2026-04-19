/// @file test_snapshot.cpp
/// @brief Unit tests for `encodeSnapshot` / `decodeSnapshot`.
///
/// The core invariants tested:
///   1. An encode → decode round-trip reproduces every replicated
///      component of every entity within the quantiser's error bound.
///   2. Entities present on the source world but absent from the
///      replication list contribute no bytes.
///   3. The tick header round-trips exactly.
///   4. An entity with only a subset of replicated components emits a
///      correct partial mask and decodes without touching the other
///      components on the receiver.
///   5. An empty world produces a well-formed (but tiny) snapshot.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>

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

/// Build a fresh world with `game::Transform`, `game::Velocity`, and
/// `game::PlayerInput` registered. Matches the setup the production
/// client and server use.
World makeWorld() {
    World w;
    w.registerComponent<game::Transform>();
    w.registerComponent<game::Velocity>();
    w.registerComponent<game::PlayerInput>();
    return w;
}

float positionStep() {
    const uint32_t steps = (1u << engine::net::kPositionBits) - 1u;
    return (2.0f * engine::net::kWorldExtent) / static_cast<float>(steps);
}

float angleStep() {
    constexpr float kTwoPi = 6.28318530717958647692f;
    const uint32_t steps = (1u << engine::net::kAngleBits) - 1u;
    return kTwoPi / static_cast<float>(steps);
}

}  // namespace

TEST_CASE("Snapshot: tick roundtrips and empty world encodes a minimal header") {
    World w = makeWorld();

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(w, /*tick=*/42, out);
    const auto& bytes = out.finish();

    // Empty world: varint(42) + varint(0) = 2 bytes.
    CHECK(bytes.size() == 2u);

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0;
    World recv = makeWorld();
    CHECK(engine::net::decodeSnapshot<GameReplication>(in, recv, tick));
    CHECK(tick == 42u);
}

TEST_CASE("Snapshot: single entity with full component set roundtrips") {
    World src = makeWorld();
    const EntityId e = src.createEntity();
    src.addComponent<game::Transform>(e, {1.25f, -3.5f, 1.0f});
    src.addComponent<game::Velocity>(e, {0.5f, -0.25f});

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, /*tick=*/7, out);
    const auto& bytes = out.finish();

    // Receiving world: same registrations, same entity id created in
    // the same order (so `createEntity()` returns the same EntityId).
    World dst = makeWorld();
    const EntityId eDst = dst.createEntity();
    CHECK(eDst == e);  // sanity: ECS should return id 0 first from both
    dst.addComponent<game::Transform>(eDst, {0.0f, 0.0f, 0.0f});

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0;
    REQUIRE(engine::net::decodeSnapshot<GameReplication>(in, dst, tick));
    CHECK(tick == 7u);

    const float pStep = positionStep();
    const float aStep = angleStep();

    const auto& tr = dst.getComponent<game::Transform>(eDst);
    CHECK(std::fabs(tr.x - 1.25f)   <= pStep);
    CHECK(std::fabs(tr.y - (-3.5f)) <= pStep);
    CHECK(std::fabs(tr.rotation - 1.0f) <= aStep);

    // Velocity was missing on dst; decode should have added it.
    CHECK(dst.hasComponent<game::Velocity>(eDst));
    const auto& v = dst.getComponent<game::Velocity>(eDst);
    CHECK(v.vx == doctest::Approx(0.5f));
    CHECK(v.vy == doctest::Approx(-0.25f));
}

TEST_CASE("Snapshot: partial component set encodes a minimal mask") {
    World src = makeWorld();
    const EntityId e = src.createEntity();
    src.addComponent<game::Transform>(e, {0.1f, 0.2f, 0.0f});
    // No Velocity, no PlayerInput.

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, /*tick=*/0, out);
    const auto& bytes = out.finish();

    // Expected size breakdown:
    //   tick varint            : 1 byte  (0)
    //   count varint           : 1 byte  (1)
    //   entity id varint       : 1 byte  (0)
    //   component mask         : 1 byte  (0b001)
    //   Transform              : 5 bytes (40 bits)
    // Total: 9 bytes.
    CHECK(bytes.size() == 9u);

    // Verify the mask byte is 0b001.
    //   Layout: [tick=0][count=1][id=0][mask][transform...]
    CHECK(bytes[3] == 0b001);

    // Decode: Velocity must NOT appear on the receiver's entity.
    World dst = makeWorld();
    const EntityId eDst = dst.createEntity();
    CHECK(eDst == e);

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0;
    REQUIRE(engine::net::decodeSnapshot<GameReplication>(in, dst, tick));
    CHECK(dst.hasComponent<game::Transform>(eDst));
    CHECK_FALSE(dst.hasComponent<game::Velocity>(eDst));
    CHECK_FALSE(dst.hasComponent<game::PlayerInput>(eDst));
}

TEST_CASE("Snapshot: multiple entities with mixed components") {
    World src = makeWorld();
    const EntityId a = src.createEntity();
    const EntityId b = src.createEntity();
    const EntityId c = src.createEntity();

    src.addComponent<game::Transform>(a, {-0.5f, 0.5f, 0.0f});
    src.addComponent<game::Velocity>(a, {1.0f, 0.0f});

    src.addComponent<game::Transform>(b, {2.0f, -2.0f, 3.14f});

    // entity c: PlayerInput only (server echoing last-known client input)
    game::PlayerInput pi{};
    pi.moveUp = true;
    pi.attack = true;
    src.addComponent<game::PlayerInput>(c, pi);

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, /*tick=*/123, out);
    const auto& bytes = out.finish();

    World dst = makeWorld();
    const EntityId aDst = dst.createEntity();
    const EntityId bDst = dst.createEntity();
    const EntityId cDst = dst.createEntity();
    CHECK(aDst == a);
    CHECK(bDst == b);
    CHECK(cDst == c);

    BitReader in(bytes.data(), bytes.size());
    uint32_t tick = 0;
    REQUIRE(engine::net::decodeSnapshot<GameReplication>(in, dst, tick));
    CHECK(tick == 123u);

    const float pStep = positionStep();

    CHECK(dst.hasComponent<game::Transform>(aDst));
    CHECK(dst.hasComponent<game::Velocity>(aDst));
    CHECK(std::fabs(dst.getComponent<game::Transform>(aDst).x - (-0.5f)) <= pStep);

    CHECK(dst.hasComponent<game::Transform>(bDst));
    CHECK_FALSE(dst.hasComponent<game::Velocity>(bDst));

    CHECK(dst.hasComponent<game::PlayerInput>(cDst));
    CHECK_FALSE(dst.hasComponent<game::Transform>(cDst));
    const auto& piRecv = dst.getComponent<game::PlayerInput>(cDst);
    CHECK(piRecv.moveUp    == true);
    CHECK(piRecv.moveDown  == false);
    CHECK(piRecv.attack    == true);
    CHECK(piRecv.dodge     == false);
}

TEST_CASE("Snapshot: entity without replicated components contributes nothing") {
    World src = makeWorld();
    (void)src.createEntity();
    // Don't add any replicated component. The entity is "alive" in the
    // ECS sense but has no on-wire representation.

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, 1, out);
    const auto& bytes = out.finish();

    // Same size as an empty world: just tick + count(0).
    CHECK(bytes.size() == 2u);
}

TEST_CASE("Snapshot: truncated input fails gracefully") {
    World src = makeWorld();
    const EntityId e = src.createEntity();
    src.addComponent<game::Transform>(e, {0.0f, 0.0f, 0.0f});

    BitWriter out;
    engine::net::encodeSnapshot<GameReplication>(src, 1, out);
    const auto& full = out.finish();

    // Truncate to 4 bytes — definitely shorter than a full entity block.
    std::vector<uint8_t> truncated(full.begin(), full.begin() + 4);

    World dst = makeWorld();
    dst.createEntity();

    BitReader in(truncated.data(), truncated.size());
    uint32_t tick = 0;
    const bool ok = engine::net::decodeSnapshot<GameReplication>(in, dst, tick);
    CHECK_FALSE(ok);
}
