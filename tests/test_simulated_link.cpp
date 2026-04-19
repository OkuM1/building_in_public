// Phase 4f: SimulatedLink — in-process latency/jitter/loss/reordering.

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/net/PacketHeader.h"
#include "engine/net/ReliableEndpoint.h"
#include "engine/net/SimulatedLink.h"

using namespace engine::net;

namespace {

std::vector<std::uint8_t> makePacket(std::uint16_t tag) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(tag >> 8));
    p.push_back(static_cast<std::uint8_t>(tag & 0xFF));
    return p;
}

std::uint16_t readTag(const std::vector<std::uint8_t>& p) {
    REQUIRE(p.size() >= 2);
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

}  // namespace

TEST_CASE("SimulatedLink: lossless, no-jitter link delivers exactly one-way-latency later") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 50;
    SimulatedLink link{cfg};

    auto p = makePacket(1);
    CHECK(link.send(p.data(), p.size(), /*now=*/0));

    // Nothing is ready yet.
    CHECK(link.receive(10).empty());
    CHECK(link.receive(49).empty());

    // At exactly 50 ms it's delivered.
    auto got = link.receive(50);
    REQUIRE(got.size() == 1);
    CHECK(readTag(got[0]) == 1);
    CHECK(link.deliveredCount() == 1);
    CHECK(link.droppedCount() == 0);
}

TEST_CASE("SimulatedLink: all packets lost at 100% loss probability") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs  = 10;
    cfg.lossProbability  = 1.0;
    SimulatedLink link{cfg};

    for (std::uint16_t i = 0; i < 50; ++i) {
        auto p = makePacket(i);
        CHECK_FALSE(link.send(p.data(), p.size(), 0));
    }
    CHECK(link.inFlight() == 0);
    CHECK(link.receive(1000).empty());
    CHECK(link.droppedCount() == 50);
    CHECK(link.sentCount() == 50);
}

TEST_CASE("SimulatedLink: loss rate is approximately honoured over a large sample") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 10;
    cfg.lossProbability = 0.25;
    cfg.seed            = 0xDEADBEEF;
    SimulatedLink link{cfg};

    constexpr int kN = 10000;
    for (int i = 0; i < kN; ++i) {
        std::uint8_t byte = 0;
        link.send(&byte, 1, 0);
    }
    const double observedLoss =
        static_cast<double>(link.droppedCount()) / static_cast<double>(kN);
    // 25% +/- 2%.
    CHECK(observedLoss > 0.23);
    CHECK(observedLoss < 0.27);
}

TEST_CASE("SimulatedLink: jitter can reorder packets") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 100;
    cfg.jitterMs        = 80;
    cfg.seed            = 1;
    SimulatedLink link{cfg};

    // Send a burst at t=0. With jitter +/-80 ms any permutation is
    // possible; run long enough to guarantee at least one reorder.
    constexpr int kN = 200;
    for (std::uint16_t i = 0; i < kN; ++i) {
        auto p = makePacket(i);
        link.send(p.data(), p.size(), 0);
    }

    // Drain all. Delivery order should NOT be strictly increasing.
    auto delivered = link.receive(10000);
    REQUIRE(delivered.size() == kN);

    bool anyReorder = false;
    std::uint16_t prev = readTag(delivered[0]);
    for (std::size_t i = 1; i < delivered.size(); ++i) {
        std::uint16_t cur = readTag(delivered[i]);
        if (cur < prev) {
            anyReorder = true;
            break;
        }
        prev = cur;
    }
    CHECK(anyReorder);
}

TEST_CASE("SimulatedLink: equal-delivery-time packets are delivered FIFO") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 20;
    cfg.jitterMs        = 0;  // no jitter => all packets tied
    SimulatedLink link{cfg};

    for (std::uint16_t i = 0; i < 10; ++i) {
        auto p = makePacket(i);
        link.send(p.data(), p.size(), 0);
    }
    auto got = link.receive(20);
    REQUIRE(got.size() == 10);
    for (std::uint16_t i = 0; i < 10; ++i) {
        CHECK(readTag(got[i]) == i);
    }
}

TEST_CASE("SimulatedLink: partial drain leaves the rest in flight") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 50;
    SimulatedLink link{cfg};

    auto a = makePacket(1);
    link.send(a.data(), a.size(), 0);    // deliver at 50
    auto b = makePacket(2);
    link.send(b.data(), b.size(), 100);  // deliver at 150

    auto first = link.receive(80);
    REQUIRE(first.size() == 1);
    CHECK(readTag(first[0]) == 1);
    CHECK(link.inFlight() == 1);

    auto second = link.receive(200);
    REQUIRE(second.size() == 1);
    CHECK(readTag(second[0]) == 2);
    CHECK(link.inFlight() == 0);
}

TEST_CASE("SimulatedLink: same seed, same call sequence, same outcomes") {
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 50;
    cfg.jitterMs        = 30;
    cfg.lossProbability = 0.2;
    cfg.seed            = 42;

    auto run = [&]() {
        SimulatedLink link{cfg};
        std::vector<std::uint16_t> delivered;
        for (std::uint16_t i = 0; i < 500; ++i) {
            auto p = makePacket(i);
            link.send(p.data(), p.size(), /*now=*/i);
        }
        auto got = link.receive(100000);
        for (auto& pkt : got) delivered.push_back(readTag(pkt));
        return delivered;
    };

    auto a = run();
    auto b = run();
    CHECK(a == b);
}

TEST_CASE("SimulatedLink: end-to-end with PacketHeader over a lossy link preserves protocol framing") {
    // Sanity check that PacketHeader round-trips through the link
    // intact. This is the seam the rest of Phase 4 sits on top of.
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 20;
    cfg.lossProbability = 0.1;
    cfg.seed            = 99;
    SimulatedLink link{cfg};

    std::array<std::uint8_t, kPacketHeaderBytes> buf{};
    PacketHeader h{};
    h.sequence = 7;
    h.ack      = 42;
    h.ackBits  = 0xABCDEF01;
    writePacketHeader(h, buf.data(), buf.size());

    // Send 100 copies through the lossy link; every delivered one
    // must parse identically.
    for (int i = 0; i < 100; ++i) {
        link.send(buf.data(), buf.size(), /*now=*/i);
    }
    auto delivered = link.receive(10000);
    CHECK(delivered.size() > 0);
    CHECK(delivered.size() < 100);  // some got dropped

    for (auto& pkt : delivered) {
        REQUIRE(pkt.size() == kPacketHeaderBytes);
        PacketHeader parsed{};
        REQUIRE(readPacketHeader(pkt.data(), pkt.size(), parsed));
        CHECK(parsed.sequence == 7);
        CHECK(parsed.ack      == 42);
        CHECK(parsed.ackBits  == 0xABCDEF01u);
    }
}

TEST_CASE("SimulatedLink: 150 ms / 5% profile — roadmap milestone shape") {
    // This doesn't plug the whole stack together yet (glue commit
    // follows 4f) but it pins the link's numbers at the target
    // profile so the closeout test has a known-good baseline.
    LinkConfig cfg;
    cfg.oneWayLatencyMs = 75;    // 150 ms RTT
    cfg.jitterMs        = 10;
    cfg.lossProbability = 0.05;
    cfg.seed            = 0xBADC0DE;
    SimulatedLink link{cfg};

    constexpr int kN = 2000;
    for (int i = 0; i < kN; ++i) {
        std::uint8_t byte = static_cast<std::uint8_t>(i & 0xFF);
        link.send(&byte, 1, /*now=*/i);
    }

    // Drain after ample time.
    auto got = link.receive(1000000);
    const double lossRate =
        1.0 - static_cast<double>(got.size()) / static_cast<double>(kN);
    CHECK(lossRate > 0.035);
    CHECK(lossRate < 0.065);
}
