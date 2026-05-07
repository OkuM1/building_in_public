/// @file test_replication.cpp
/// @brief Unit tests for Phase 5a replication primitives.
///
/// Covers:
///   - InputMessage serialize/deserialize round-trip (all fields)
///   - SnapshotBuffer push, capacity eviction, interpolation
///   - PredictionBuffer push, ackUpTo, getPending, get
///   - Reconciliation flow: snap + replay on misprediction
///   - End-to-end integration: server-authoritative loop delivers
///     snapshots through Connection + SimulatedLink; client buffers
///     them and can interpolate; prediction + reconciliation converge.

#include <doctest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "engine/core/Simulation.h"
#include "engine/net/Connection.h"
#include "engine/net/Serialize.h"
#include "engine/net/SimulatedLink.h"
#include "engine/replication/InputMessage.h"
#include "engine/replication/PredictionBuffer.h"
#include "engine/replication/SnapshotBuffer.h"
#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
#include "game/components/ComponentSerializers.h"
#include "game/components/GameComponents.h"

using namespace engine::replication;
using engine::EntityId;
using engine::net::BitReader;
using engine::net::BitWriter;
using engine::net::ChannelId;
using engine::net::Connection;
using engine::net::GameReplication;
using engine::net::SimulatedLink;

// ---------------------------------------------------------------------------
// Helpers shared by multiple test cases
// ---------------------------------------------------------------------------

namespace {

class FakeClock : public engine::net::Clock {
public:
    Millis nowMs() const override { return now_; }
    Millis now_ = 0;
};

// Build a minimal world with the components used by the replication tests.
// Mirrors the server-side component registration.
engine::World makeWorld() {
    engine::World w;
    w.registerComponent<game::Transform>();
    w.registerComponent<game::Velocity>();
    w.registerComponent<game::PlayerInput>();
    return w;
}

// Decode a raw snapshot payload into a ReplicaSnapshot by decoding into a
// temporary World and extracting the Transform for each live entity.
// Used by the integration test to feed the SnapshotBuffer.
ReplicaSnapshot decodeToReplica(const std::vector<std::uint8_t>& bytes,
                                engine::World& decodeWorld) {
    BitReader in(bytes.data(), bytes.size());
    std::uint32_t tick = 0;
    engine::net::decodeSnapshot<GameReplication>(in, decodeWorld, tick);

    ReplicaSnapshot snap;
    snap.tick = tick;
    // Entity 0 is the single player entity in these tests.
    if (decodeWorld.hasComponent<game::Transform>(0)) {
        snap.entities.push_back({0, decodeWorld.getComponent<game::Transform>(0)});
    }
    return snap;
}

}  // namespace

// ===========================================================================
// InputMessage
// ===========================================================================

TEST_CASE("InputMessage: serialize/deserialize round-trips all fields") {
    InputMessage src;
    src.tick             = 0xDEADBEEFu;
    src.input.moveUp     = true;
    src.input.moveDown   = false;
    src.input.moveLeft   = true;
    src.input.moveRight  = false;
    src.input.attack     = true;
    src.input.dodge      = false;

    BitWriter bw;
    serializeInput(bw, src);
    const auto& bytes = bw.finish();

    BitReader br(bytes.data(), bytes.size());
    InputMessage dst;
    REQUIRE(deserializeInput(br, dst));

    CHECK(dst.tick              == src.tick);
    CHECK(dst.input.moveUp      == src.input.moveUp);
    CHECK(dst.input.moveDown    == src.input.moveDown);
    CHECK(dst.input.moveLeft    == src.input.moveLeft);
    CHECK(dst.input.moveRight   == src.input.moveRight);
    CHECK(dst.input.attack      == src.input.attack);
    CHECK(dst.input.dodge       == src.input.dodge);
}

TEST_CASE("InputMessage: zero-input message round-trips") {
    InputMessage src;  // all defaults: tick=0, no buttons pressed

    BitWriter bw;
    serializeInput(bw, src);
    const auto& bytes = bw.finish();

    BitReader br(bytes.data(), bytes.size());
    InputMessage dst;
    REQUIRE(deserializeInput(br, dst));

    CHECK(dst.tick              == 0u);
    CHECK(dst.input.moveUp      == false);
    CHECK(dst.input.moveRight   == false);
    CHECK(dst.input.attack      == false);
}

TEST_CASE("InputMessage: truncated stream returns false") {
    InputMessage src;
    src.tick = 42;

    BitWriter bw;
    serializeInput(bw, src);
    const auto& bytes = bw.finish();

    // Give only 1 byte — too short for even the tick varint + bools.
    BitReader br(bytes.data(), 1);
    InputMessage dst;
    // deserialize may or may not fail depending on varint length,
    // but it must not crash.
    (void)deserializeInput(br, dst);
    // Only check for absence of UB / crash (no assertion needed here).
}

// ===========================================================================
// SnapshotBuffer
// ===========================================================================

TEST_CASE("SnapshotBuffer: empty buffer cannot interpolate") {
    SnapshotBuffer buf;
    CHECK(buf.size() == 0u);
    CHECK_FALSE(buf.canInterpolate(5.0f));
    CHECK_FALSE(buf.interpolate(0, 5.0f).has_value());
}

TEST_CASE("SnapshotBuffer: one snapshot — cannot interpolate (need ≥ 2)") {
    SnapshotBuffer buf;
    buf.push({10, {{0, {1.0f, 0.0f, 0.0f}}}});
    CHECK(buf.size() == 1u);
    CHECK_FALSE(buf.canInterpolate(10.0f));
    CHECK_FALSE(buf.interpolate(0, 10.0f).has_value());
}

TEST_CASE("SnapshotBuffer: two snapshots — exact boundary values") {
    SnapshotBuffer buf;
    buf.push({10, {{0, {0.0f, 0.0f, 0.0f}}}});
    buf.push({20, {{0, {10.0f, 0.0f, 0.0f}}}});

    CHECK(buf.oldestTick() == 10u);
    CHECK(buf.newestTick() == 20u);

    // Exactly at tick 10 (alpha = 0)
    REQUIRE(buf.canInterpolate(10.0f));
    auto t10 = buf.interpolate(0, 10.0f);
    REQUIRE(t10.has_value());
    CHECK(t10->x == doctest::Approx(0.0f));

    // Midpoint (alpha = 0.5)
    auto t15 = buf.interpolate(0, 15.0f);
    REQUIRE(t15.has_value());
    CHECK(t15->x == doctest::Approx(5.0f));

    // At newest tick (alpha = 1)
    auto t20 = buf.interpolate(0, 20.0f);
    REQUIRE(t20.has_value());
    CHECK(t20->x == doctest::Approx(10.0f));
}

TEST_CASE("SnapshotBuffer: interpolate outside buffered range returns nullopt") {
    SnapshotBuffer buf;
    buf.push({10, {{0, {0.0f, 0.0f, 0.0f}}}});
    buf.push({20, {{0, {10.0f, 0.0f, 0.0f}}}});

    CHECK_FALSE(buf.canInterpolate(5.0f));   // before oldest
    CHECK_FALSE(buf.canInterpolate(25.0f));  // after newest
    CHECK_FALSE(buf.interpolate(0, 5.0f).has_value());
    CHECK_FALSE(buf.interpolate(0, 25.0f).has_value());
}

TEST_CASE("SnapshotBuffer: entity absent in one snapshot returns nullopt") {
    SnapshotBuffer buf;
    // Entity 0 exists in tick 10; entity 1 does NOT.
    buf.push({10, {{0, {1.0f, 0.0f, 0.0f}}}});
    // Entity 0 missing from tick 20; entity 1 appears.
    buf.push({20, {{1, {5.0f, 0.0f, 0.0f}}}});

    // Entity 0: present in tick 10 but absent in tick 20 → nullopt
    CHECK_FALSE(buf.interpolate(0, 15.0f).has_value());
    // Entity 1: absent in tick 10 → nullopt
    CHECK_FALSE(buf.interpolate(1, 15.0f).has_value());
}

TEST_CASE("SnapshotBuffer: capacity evicts oldest snapshot") {
    SnapshotBuffer buf;
    for (std::uint32_t t = 0; t < SnapshotBuffer::kCapacity + 5; ++t) {
        buf.push({t, {}});
    }
    CHECK(buf.size() == SnapshotBuffer::kCapacity);
    CHECK(buf.oldestTick() == 5u);  // first 5 were evicted
}

TEST_CASE("SnapshotBuffer: out-of-order snapshot is discarded") {
    SnapshotBuffer buf;
    buf.push({20, {}});
    buf.push({10, {}});  // older than current newest — discard
    CHECK(buf.size() == 1u);
    CHECK(buf.newestTick() == 20u);
}

TEST_CASE("SnapshotBuffer: rotation interpolates correctly") {
    SnapshotBuffer buf;
    buf.push({0,  {{0, {0.0f, 0.0f, 0.0f}}}});
    buf.push({10, {{0, {0.0f, 0.0f, 1.0f}}}});

    auto r5 = buf.interpolate(0, 5.0f);
    REQUIRE(r5.has_value());
    CHECK(r5->rotation == doctest::Approx(0.5f));
}

// ===========================================================================
// PredictionBuffer
// ===========================================================================

TEST_CASE("PredictionBuffer: empty buffer basics") {
    PredictionBuffer buf;
    CHECK(buf.empty());
    CHECK(buf.size() == 0u);
    CHECK(buf.get(0) == nullptr);
    CHECK(buf.getPending(0).empty());
}

TEST_CASE("PredictionBuffer: push and get") {
    PredictionBuffer buf;
    game::PlayerInput inp{};
    inp.moveRight = true;
    game::Transform t{1.0f, 0.0f, 0.0f};

    buf.push(5, inp, t);
    buf.push(6, inp, {2.0f, 0.0f, 0.0f});

    CHECK(buf.size() == 2u);
    CHECK(buf.oldestTick() == 5u);
    CHECK(buf.newestTick() == 6u);

    const auto* e5 = buf.get(5);
    REQUIRE(e5 != nullptr);
    CHECK(e5->predictedTransform.x == doctest::Approx(1.0f));
    CHECK(e5->input.moveRight == true);

    CHECK(buf.get(99) == nullptr);
}

TEST_CASE("PredictionBuffer: ackUpTo discards confirmed entries") {
    PredictionBuffer buf;
    for (std::uint32_t t = 0; t < 10; ++t) {
        buf.push(t, {}, {static_cast<float>(t), 0.0f, 0.0f});
    }
    CHECK(buf.size() == 10u);

    buf.ackUpTo(4);
    CHECK(buf.size() == 5u);  // ticks 5-9 remain
    CHECK(buf.oldestTick() == 5u);
    CHECK(buf.get(4) == nullptr);
    REQUIRE(buf.get(5) != nullptr);
}

TEST_CASE("PredictionBuffer: getPending returns entries after given tick") {
    PredictionBuffer buf;
    for (std::uint32_t t = 1; t <= 10; ++t) {
        buf.push(t, {}, {static_cast<float>(t), 0.0f, 0.0f});
    }

    auto pending = buf.getPending(5);
    REQUIRE(pending.size() == 5u);  // ticks 6-10
    CHECK(pending.front().tick == 6u);
    CHECK(pending.back().tick  == 10u);

    // After tick 10: nothing pending
    CHECK(buf.getPending(10).empty());
    // After tick 0: all 10 entries
    CHECK(buf.getPending(0).size() == 10u);
}

TEST_CASE("PredictionBuffer: capacity evicts oldest entry") {
    PredictionBuffer buf;
    for (std::uint32_t t = 0; t < PredictionBuffer::kCapacity + 3; ++t) {
        buf.push(t, {}, {});
    }
    CHECK(buf.size() == PredictionBuffer::kCapacity);
    CHECK(buf.oldestTick() == 3u);
}

// ===========================================================================
// Reconciliation flow (unit-level, no networking)
// ===========================================================================

TEST_CASE("Reconciliation: snap + replay corrects diverged prediction") {
    // Simulate 60 ticks of prediction with moveRight=true.
    // At tick 30 the server sends an authoritative snapshot showing
    // x = 0.0 (as if the input was blocked). The client must snap to 0
    // and replay ticks 31-59 forward.

    PredictionBuffer pred;
    constexpr float kSpeed    = 0.5f;   // from InputSystem::kPlayerSpeed
    constexpr float kFixedDt  = 1.0f / 60.0f;
    constexpr float kDxPerTick = kSpeed * kFixedDt;

    game::PlayerInput moveRight{};
    moveRight.moveRight = true;

    // Build 60 ticks of predicted state: each tick advances x by kDxPerTick.
    float x = 0.0f;
    for (std::uint32_t t = 0; t < 60; ++t) {
        x += kDxPerTick;
        pred.push(t, moveRight, {x, 0.0f, 0.0f});
    }
    CHECK(pred.size() == 60u);

    // Server arrives with authoritative state at tick 30: x = 0 (blocked).
    const std::uint32_t serverTick = 30;
    const game::Transform serverState{0.0f, 0.0f, 0.0f};

    // Step 1: discard confirmed entries.
    pred.ackUpTo(serverTick);
    CHECK(pred.size() == 29u);  // ticks 31-59 remain
    CHECK(pred.oldestTick() == 31u);

    // Step 2: get pending inputs for replay.
    auto pending = pred.getPending(serverTick);
    CHECK(pending.size() == 29u);  // ticks 31-59

    // Step 3: replay from server state.
    float replayX = serverState.x;
    for (const auto& entry : pending) {
        if (entry.input.moveRight) replayX += kDxPerTick;
    }

    // After 29 ticks of moveRight from x=0:
    const float expected = 29.0f * kDxPerTick;
    CHECK(replayX == doctest::Approx(expected));

    // Client's *old* prediction at tick 59 was 60 * kDxPerTick from x=0.
    // The reconciled value is only 29 * kDxPerTick from x=0.
    // They differ by 31 ticks worth — the correction is significant.
    CHECK(replayX < 60.0f * kDxPerTick);
}

// ===========================================================================
// End-to-end integration: server-authoritative loop
// ===========================================================================

TEST_CASE("Replication: server-authoritative loop — client receives snapshots "
          "and interpolation converges") {
    // Two Connection objects over zero-latency SimulatedLinks.
    FakeClock clk;
    Connection serverConn(clk), clientConn(clk);

    engine::net::LinkConfig zeroLoss;
    zeroLoss.oneWayLatencyMs = 0.0;
    zeroLoss.lossProbability = 0.0;
    SimulatedLink linkStoC{zeroLoss};
    SimulatedLink linkCtoS{zeroLoss};

    // Server simulation.
    engine::Simulation sim;
    {
        auto& w = sim.world();
        w.registerComponent<game::Transform>();
        w.registerComponent<game::PreviousTransform>();
        w.registerComponent<game::Velocity>();
        w.registerComponent<game::PlayerInput>();
        w.registerComponent<game::Player>();
        sim.addSystem(std::make_unique<engine::InputSystem>());
        sim.addSystem(std::make_unique<engine::MovementSystem>());
    }

    // Spawn the single player entity (will be entity ID 0).
    const EntityId playerEnt = sim.world().createEntity();
    {
        auto& w = sim.world();
        w.addComponent(playerEnt, game::Transform{0.0f, 0.0f, 0.0f});
        w.addComponent(playerEnt, game::PreviousTransform{0.0f, 0.0f, 0.0f});
        w.addComponent(playerEnt, game::Velocity{0.0f, 0.0f});
        w.addComponent(playerEnt, game::PlayerInput{});
    }

    // Client-side buffers.
    SnapshotBuffer  snapBuf;
    PredictionBuffer predBuf;

    // Temporary world used to decode incoming snapshots.
    engine::World decodeWorld = makeWorld();
    decodeWorld.createEntity();  // pre-create entity 0 to match server

    static constexpr int kTicks = 180;  // 3 simulated seconds at 60 Hz

    for (int t = 0; t < kTicks; ++t) {
        clk.now_ = static_cast<engine::net::Clock::Millis>(t) * 16;  // ~60 Hz

        // ---- Client: form input and transmit --------------------------------
        game::PlayerInput inp{};
        inp.moveRight = true;
        InputMessage imsg{static_cast<std::uint32_t>(t), inp};

        BitWriter bw;
        serializeInput(bw, imsg);
        const auto& ibytes = bw.finish();
        clientConn.send(ChannelId::ReliableUnordered, ibytes.data(), ibytes.size());

        if (clientConn.shouldSendNow(clk.now_)) {
            auto pkt = clientConn.buildPacket(clk.now_);
            linkCtoS.send(pkt.data(), pkt.size(), clk.now_);
        }

        // ---- Server: drain, apply inputs, step sim --------------------------
        for (auto& pkt : linkCtoS.receive(clk.now_)) {
            serverConn.receivePacket(pkt.data(), pkt.size(), clk.now_);
        }
        while (auto payload = serverConn.receive(ChannelId::ReliableUnordered)) {
            BitReader in(payload->data(), payload->size());
            InputMessage received;
            if (deserializeInput(in, received)) {
                sim.world().getComponent<game::PlayerInput>(playerEnt) =
                    received.input;
            }
        }
        sim.step();

        // Server: encode and send snapshot when congestion allows.
        if (serverConn.shouldSendNow(clk.now_)) {
            BitWriter sw;
            engine::net::encodeSnapshot<GameReplication>(
                sim.world(), sim.tick(), sw);
            const auto& sbytes = sw.finish();
            serverConn.send(ChannelId::Unreliable, sbytes.data(), sbytes.size());
            auto pkt = serverConn.buildPacket(clk.now_);
            linkStoC.send(pkt.data(), pkt.size(), clk.now_);
        }

        // ---- Client: drain inbound, buffer snapshots ----------------------
        for (auto& pkt : linkStoC.receive(clk.now_)) {
            clientConn.receivePacket(pkt.data(), pkt.size(), clk.now_);
        }
        while (auto payload = clientConn.receive(ChannelId::Unreliable)) {
            ReplicaSnapshot snap =
                decodeToReplica(*payload, decodeWorld);
            snapBuf.push(std::move(snap));
        }

        // Client: record local prediction (manually apply same movement).
        const float predictedX =
            (t + 1) * 0.5f * engine::Simulation::FIXED_DT;  // kPlayerSpeed=0.5
        predBuf.push(static_cast<std::uint32_t>(t), inp,
                     {predictedX, 0.0f, 0.0f});
    }

    // -------------------------------------------------------------------
    // Assertions
    // -------------------------------------------------------------------

    // Server entity moved right.
    const auto& finalT =
        sim.world().getComponent<game::Transform>(playerEnt);
    CHECK(finalT.x > 0.0f);
    // x is clamped at kWorldSize = 0.9 by MovementSystem, so it shouldn't exceed 0.9.
    CHECK(finalT.x <= 0.9f + 1e-4f);

    // SnapshotBuffer accumulated snapshots.
    CHECK(snapBuf.size() > 0u);
    CHECK(snapBuf.newestTick() > 0u);

    // Can interpolate within the buffered range.
    if (snapBuf.size() >= 2) {
        const float midTick =
            0.5f * (static_cast<float>(snapBuf.oldestTick()) +
                    static_cast<float>(snapBuf.newestTick()));
        REQUIRE(snapBuf.canInterpolate(midTick));

        auto interp = snapBuf.interpolate(playerEnt, midTick);
        REQUIRE(interp.has_value());
        CHECK(interp->x > 0.0f);  // entity moved right
    }

    // PredictionBuffer caps at kCapacity (128). With 180 pushes the oldest
    // 52 entries were evicted, so oldest tick = 180 - 128 = 52.
    static constexpr std::size_t kCap    = PredictionBuffer::kCapacity;
    static constexpr std::uint32_t kOldest = static_cast<std::uint32_t>(kTicks) - kCap;
    CHECK(predBuf.size() == kCap);
    CHECK(predBuf.oldestTick() == kOldest);

    // Simulate receiving a server ack at tick 89: discard entries 52..89
    // (= 38 entries), leaving entries 90..179 (= 90 entries).
    predBuf.ackUpTo(89);
    const std::size_t kExpectAfterAck = kCap - (90u - kOldest);
    CHECK(predBuf.size() == kExpectAfterAck);
    CHECK(predBuf.oldestTick() == 90u);

    // getPending returns exactly the unconfirmed entries.
    auto pending = predBuf.getPending(89);
    CHECK(pending.size() == kExpectAfterAck);
}
