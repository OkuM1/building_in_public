// Phase 4 closeout: Connection facade + stability milestone.
//
// Tests the Connection class that wires together ReliableEndpoint,
// UnreliableChannel, ReliableChannel ×2, and CongestionController.
//
// The final TEST_CASE is the Phase 4 roadmap milestone:
//   Two Connection objects exchange reliable + unreliable messages
//   through two SimulatedLinks at 75 ms one-way / 5 % loss
//   (≈ 150 ms RTT / 5 % loss) and remain stable.

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "engine/net/Connection.h"
#include "engine/net/SimulatedLink.h"

using namespace engine::net;

namespace {

class FakeClock : public Clock {
public:
    Millis nowMs() const override { return now_; }
    Millis now_ = 0;
};

Payload bytesOf(const std::string& s) {
    return Payload(s.begin(), s.end());
}

std::string stringOf(const Payload& p) {
    return std::string(p.begin(), p.end());
}

}  // namespace

// ---------------------------------------------------------------------------
// Basic unit tests
// ---------------------------------------------------------------------------

TEST_CASE("Connection: unreliable message round-trips over a zero-latency link") {
    FakeClock clk;
    Connection A(clk), B(clk);

    A.send(ChannelId::Unreliable, bytesOf("snap0"));

    REQUIRE(A.shouldSendNow(0));
    auto pkt = A.buildPacket(0);
    REQUIRE_FALSE(pkt.empty());
    REQUIRE(B.receivePacket(pkt.data(), pkt.size(), 0));

    auto m = B.receive(ChannelId::Unreliable);
    REQUIRE(m.has_value());
    CHECK(stringOf(*m) == "snap0");
    CHECK_FALSE(B.receive(ChannelId::Unreliable).has_value());
}

TEST_CASE("Connection: empty packet is valid — carries ack header only") {
    FakeClock clk;
    Connection A(clk), B(clk);

    // Nothing enqueued. buildPacket still produces a well-formed packet
    // so the peer gets acks.
    auto pkt = A.buildPacket(0);
    REQUIRE(B.receivePacket(pkt.data(), pkt.size(), 0));

    CHECK_FALSE(B.receive(ChannelId::Unreliable).has_value());
    CHECK_FALSE(B.receive(ChannelId::ReliableUnordered).has_value());
    CHECK_FALSE(B.receive(ChannelId::ReliableOrdered).has_value());
}

TEST_CASE("Connection: shouldSendNow respects congestion send-rate") {
    FakeClock clk;
    // Default goodRateHz = 30 → interval = floor(1000/30) = 33 ms.
    Connection A(clk);

    CHECK(A.shouldSendNow(0));   // can send immediately at construction
    A.buildPacket(0);            // nextSendMs_ = 33
    CHECK_FALSE(A.shouldSendNow(10));
    CHECK_FALSE(A.shouldSendNow(32));
    CHECK(A.shouldSendNow(33));
}

TEST_CASE("Connection: reliable-unordered message arrives and RTT is tracked") {
    FakeClock clk;
    Connection A(clk), B(clk);

    A.send(ChannelId::ReliableUnordered, bytesOf("event:kill"));

    // A -> B at t=0, B receives at t=75 (simulated one-way latency).
    auto pktA0 = A.buildPacket(0);
    clk.now_ = 75;
    REQUIRE(B.receivePacket(pktA0.data(), pktA0.size(), 75));

    auto m = B.receive(ChannelId::ReliableUnordered);
    REQUIRE(m.has_value());
    CHECK(stringOf(*m) == "event:kill");

    // B -> A ack: A receives at t=150.
    auto pktB0 = B.buildPacket(75);
    clk.now_ = 150;
    REQUIRE(A.receivePacket(pktB0.data(), pktB0.size(), 150));

    CHECK(A.smoothedRttMs() > 0.0);
}

TEST_CASE("Connection: reliable-ordered messages arrive in order") {
    FakeClock clk;
    Connection A(clk), B(clk);

    A.send(ChannelId::ReliableOrdered, bytesOf("lobby:0"));
    A.send(ChannelId::ReliableOrdered, bytesOf("lobby:1"));
    A.send(ChannelId::ReliableOrdered, bytesOf("lobby:2"));

    auto pkt = A.buildPacket(0);
    REQUIRE(B.receivePacket(pkt.data(), pkt.size(), 0));

    auto m0 = B.receive(ChannelId::ReliableOrdered);
    auto m1 = B.receive(ChannelId::ReliableOrdered);
    auto m2 = B.receive(ChannelId::ReliableOrdered);
    REQUIRE(m0.has_value());
    REQUIRE(m1.has_value());
    REQUIRE(m2.has_value());
    CHECK(stringOf(*m0) == "lobby:0");
    CHECK(stringOf(*m1) == "lobby:1");
    CHECK(stringOf(*m2) == "lobby:2");
    CHECK_FALSE(B.receive(ChannelId::ReliableOrdered).has_value());
}

TEST_CASE("Connection: duplicate inbound packet is silently rejected") {
    FakeClock clk;
    Connection A(clk), B(clk);

    auto pkt = A.buildPacket(0);
    CHECK(B.receivePacket(pkt.data(), pkt.size(), 0));
    CHECK_FALSE(B.receivePacket(pkt.data(), pkt.size(), 0));  // duplicate
}

TEST_CASE("Connection: truncated packet (below header size) is rejected") {
    FakeClock clk;
    Connection A(clk), B(clk);

    auto pkt = A.buildPacket(0);
    pkt.resize(5);  // truncate below kPacketHeaderBytes
    CHECK_FALSE(B.receivePacket(pkt.data(), pkt.size(), 0));
}

TEST_CASE("Connection: all three channels carry payloads in the same packet") {
    FakeClock clk;
    Connection A(clk), B(clk);

    A.send(ChannelId::Unreliable,        bytesOf("snap"));
    A.send(ChannelId::ReliableUnordered, bytesOf("evt"));
    A.send(ChannelId::ReliableOrdered,   bytesOf("lobby"));

    auto pkt = A.buildPacket(0);
    REQUIRE(B.receivePacket(pkt.data(), pkt.size(), 0));

    auto s = B.receive(ChannelId::Unreliable);
    auto e = B.receive(ChannelId::ReliableUnordered);
    auto l = B.receive(ChannelId::ReliableOrdered);
    REQUIRE(s.has_value());  CHECK(stringOf(*s) == "snap");
    REQUIRE(e.has_value());  CHECK(stringOf(*e) == "evt");
    REQUIRE(l.has_value());  CHECK(stringOf(*l) == "lobby");
}

// ---------------------------------------------------------------------------
// Phase 4 stability milestone
//
// Two Connections exchange traffic through two SimulatedLinks at the
// roadmap target profile: 75 ms one-way latency (150 ms RTT) and 5 %
// packet loss. A FakeClock steps at 33 ms (~30 Hz, the congestion
// controller's default good-mode rate).
//
// Assertions:
//   - All 100 reliable-unordered events sent by A arrive at B.
//   - A's RTT estimate is within a plausible range.
//   - System remains stable (no crash, no hang, no memory explosion).
// ---------------------------------------------------------------------------

TEST_CASE("Connection: Phase 4 stability milestone — 150 ms RTT / 5% loss") {
    FakeClock clk;

    // Independent seeded links so the test is deterministic.
    LinkConfig cfgAtoB;
    cfgAtoB.oneWayLatencyMs = 75.0;
    cfgAtoB.jitterMs        = 10.0;
    cfgAtoB.lossProbability = 0.05;
    cfgAtoB.seed            = 0xBADC0DE1;

    LinkConfig cfgBtoA = cfgAtoB;
    cfgBtoA.seed = 0xBADC0DE2;  // independent loss pattern per direction

    SimulatedLink linkAtoB{cfgAtoB};
    SimulatedLink linkBtoA{cfgBtoA};

    Connection connA(clk), connB(clk);

    // Enqueue 100 reliable events at A before the simulation begins.
    for (int i = 0; i < 100; ++i) {
        const std::string msg = "evt:" + std::to_string(i);
        connA.send(ChannelId::ReliableUnordered,
                   reinterpret_cast<const std::uint8_t*>(msg.data()),
                   msg.size());
    }

    // One simulated tick: drain inbound, then send if allowed.
    auto tick = [&](Clock::Millis now) {
        clk.now_ = now;

        // Drain A's inbound (B→A link).
        for (auto& pkt : linkBtoA.receive(now))
            connA.receivePacket(pkt.data(), pkt.size(), now);

        // Drain B's inbound (A→B link).
        for (auto& pkt : linkAtoB.receive(now))
            connB.receivePacket(pkt.data(), pkt.size(), now);

        // A sends if the congestion controller allows.
        if (connA.shouldSendNow(now)) {
            auto pkt = connA.buildPacket(now);
            linkAtoB.send(pkt.data(), pkt.size(), now);
        }

        // B sends unreliable snapshots + acks.
        if (connB.shouldSendNow(now)) {
            const std::string snap = "snap";
            connB.send(ChannelId::Unreliable,
                       reinterpret_cast<const std::uint8_t*>(snap.data()),
                       snap.size());
            auto pkt = connB.buildPacket(now);
            linkBtoA.send(pkt.data(), pkt.size(), now);
        }
    };

    // 12 s active run + 3 s drain to clear all in-flight retransmits.
    constexpr Clock::Millis kStepMs  = 33;
    constexpr Clock::Millis kRunMs   = 12000;
    constexpr Clock::Millis kDrainMs = 3000;

    for (Clock::Millis t = 0; t < kRunMs; t += kStepMs)         tick(t);
    for (Clock::Millis t = kRunMs; t < kRunMs + kDrainMs; t += kStepMs) tick(t);

    // All 100 reliable events must have arrived at B.
    int delivered = 0;
    while (connB.receive(ChannelId::ReliableUnordered).has_value()) ++delivered;
    CHECK(delivered == 100);

    // RTT estimate should be in the neighbourhood of 150 ms.
    CHECK(connA.smoothedRttMs() > 50.0);
    CHECK(connA.smoothedRttMs() < 500.0);

    // The links experienced real loss and real delivery.
    CHECK(linkAtoB.droppedCount() > 0);
    CHECK(linkAtoB.deliveredCount() > 0);
    CHECK(linkBtoA.droppedCount() > 0);
    CHECK(linkBtoA.deliveredCount() > 0);
}
