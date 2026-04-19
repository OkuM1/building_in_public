// Phase 4b: packet header, sequence buffer, and ReliableEndpoint.

#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "engine/net/PacketHeader.h"
#include "engine/net/ReliableEndpoint.h"
#include "engine/net/SequenceBuffer.h"

using namespace engine::net;

// ---------------------------------------------------------------------------
// PacketHeader
// ---------------------------------------------------------------------------

TEST_CASE("PacketHeader round-trips through wire bytes") {
    PacketHeader a;
    a.sequence = 0x1234;
    a.ack      = 0xABCD;
    a.ackBits  = 0xDEADBEEFu;

    std::array<std::uint8_t, kPacketHeaderBytes> buf{};
    REQUIRE(writePacketHeader(a, buf.data(), buf.size()));

    PacketHeader b;
    REQUIRE(readPacketHeader(buf.data(), buf.size(), b));
    CHECK(b.protocolId == kProtocolId);
    CHECK(b.sequence == a.sequence);
    CHECK(b.ack == a.ack);
    CHECK(b.ackBits == a.ackBits);
}

TEST_CASE("PacketHeader rejects a wrong protocol id") {
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};
    // Fill with a valid header, then corrupt the magic.
    PacketHeader a;
    writePacketHeader(a, buf.data(), buf.size());
    buf[0] ^= 0xFF;

    PacketHeader b;
    CHECK_FALSE(readPacketHeader(buf.data(), buf.size(), b));
}

TEST_CASE("PacketHeader write rejects a too-small buffer") {
    PacketHeader a;
    std::array<std::uint8_t, kPacketHeaderBytes - 1> buf{};
    CHECK_FALSE(writePacketHeader(a, buf.data(), buf.size()));
}

TEST_CASE("seqGreater handles 16-bit wrap-around") {
    CHECK(seqGreater(5, 3));
    CHECK_FALSE(seqGreater(3, 5));
    CHECK(seqGreater(0, 0xFFFFu));    // 0 is newer than 65535 (just wrapped)
    CHECK_FALSE(seqGreater(0xFFFFu, 0));
    CHECK_FALSE(seqGreater(5, 5));
}

// ---------------------------------------------------------------------------
// SequenceBuffer
// ---------------------------------------------------------------------------

TEST_CASE("SequenceBuffer insert / find / overwrite") {
    SequenceBuffer<int, 16> buf;
    auto& slot = buf.insert(3);
    slot = 42;
    REQUIRE(buf.find(3) != nullptr);
    CHECK(*buf.find(3) == 42);

    // Sequence 3 + 16 = 19 hashes to the same slot. Inserting it
    // must orphan the old sequence.
    auto& slot2 = buf.insert(19);
    slot2 = 99;
    CHECK(buf.find(3) == nullptr);          // evicted
    CHECK(buf.find(19) != nullptr);
    CHECK(*buf.find(19) == 99);
}

// ---------------------------------------------------------------------------
// ReliableEndpoint
// ---------------------------------------------------------------------------

namespace {
// Test clock with manual time advancement.
class FakeClock : public Clock {
public:
    Millis nowMs() const override { return now_; }
    void advance(Millis ms) { now_ += ms; }
    Millis now_ = 0;
};
}  // namespace

TEST_CASE("ReliableEndpoint emits sequential sequences and a valid header") {
    FakeClock clk;
    ReliableEndpoint ep(clk);

    std::array<std::uint8_t, kPacketHeaderBytes> buf{};
    CHECK(ep.writeOutboundHeader(buf.data(), buf.size()) == 0);
    CHECK(ep.writeOutboundHeader(buf.data(), buf.size()) == 1);
    CHECK(ep.writeOutboundHeader(buf.data(), buf.size()) == 2);

    PacketHeader h;
    REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
    CHECK(h.sequence == 2);
    // No packets received yet → ack must be 0 and ackBits must be 0.
    CHECK(h.ack == 0);
    CHECK(h.ackBits == 0);
}

TEST_CASE("ReliableEndpoint round-trip: two peers hand-delivering headers") {
    FakeClock clkA, clkB;
    ReliableEndpoint A(clkA), B(clkB);

    std::vector<std::uint16_t> acked;

    // A -> B: first packet at t=0.
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};
    const auto seqA0 = A.writeOutboundHeader(buf.data(), buf.size());
    CHECK(seqA0 == 0);
    {
        PacketHeader h;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
        CHECK(B.processInboundHeader(h, acked));
        CHECK(acked.empty());  // A has sent nothing that B had previously acked
    }

    // 30 ms later, B -> A: this is the ack that resolves seqA0.
    clkA.advance(30);
    clkB.advance(30);
    const auto seqB0 = B.writeOutboundHeader(buf.data(), buf.size());
    CHECK(seqB0 == 0);
    {
        PacketHeader h;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
        CHECK(A.processInboundHeader(h, acked));
        REQUIRE(acked.size() == 1);
        CHECK(acked[0] == seqA0);
    }

    // A should now have an RTT estimate ~= 30 ms.
    CHECK(A.smoothedRttMs() == doctest::Approx(30.0));
    // B has no RTT yet — B has received one packet from A but A didn't
    // ack it (A had nothing to ack when it was sent).
    CHECK(B.smoothedRttMs() == 0.0);
}

TEST_CASE("ReliableEndpoint ackBits reports multiple received packets") {
    FakeClock clkA, clkB;
    ReliableEndpoint A(clkA), B(clkB);
    std::vector<std::uint16_t> acked;
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};

    // A sends 5 packets; B receives all.
    for (int i = 0; i < 5; ++i) {
        A.writeOutboundHeader(buf.data(), buf.size());
        PacketHeader h;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
        REQUIRE(B.processInboundHeader(h, acked));
    }

    // B sends one back. Its header should ack all 5 of A's sends.
    B.writeOutboundHeader(buf.data(), buf.size());
    PacketHeader h;
    REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
    CHECK(h.ack == 4);
    // ackBits covers seqs 3,2,1,0 (bits 0..3 set).
    CHECK((h.ackBits & 0xFu) == 0xFu);

    REQUIRE(A.processInboundHeader(h, acked));
    std::sort(acked.begin(), acked.end());
    REQUIRE(acked.size() == 5);
    for (std::uint16_t i = 0; i < 5; ++i) CHECK(acked[i] == i);
}

TEST_CASE("ReliableEndpoint ackBits survives dropped packets in the middle") {
    FakeClock clkA, clkB;
    ReliableEndpoint A(clkA), B(clkB);
    std::vector<std::uint16_t> acked;
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};

    // A sends 5 packets. B drops seq 2.
    for (int i = 0; i < 5; ++i) {
        A.writeOutboundHeader(buf.data(), buf.size());
        PacketHeader h;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
        if (i == 2) continue;            // dropped
        REQUIRE(B.processInboundHeader(h, acked));
    }

    // B sends. ack = 4, ackBits for {3,1,0} set, bit for 2 clear.
    B.writeOutboundHeader(buf.data(), buf.size());
    PacketHeader h;
    REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
    CHECK(h.ack == 4);
    CHECK((h.ackBits & 0x1u) == 0x1u);   // seq 3
    CHECK((h.ackBits & 0x2u) == 0x0u);   // seq 2 missing
    CHECK((h.ackBits & 0x4u) == 0x4u);   // seq 1
    CHECK((h.ackBits & 0x8u) == 0x8u);   // seq 0

    REQUIRE(A.processInboundHeader(h, acked));
    std::sort(acked.begin(), acked.end());
    REQUIRE(acked.size() == 4);
    const std::vector<std::uint16_t> expect{0, 1, 3, 4};
    CHECK(acked == expect);
}

TEST_CASE("ReliableEndpoint duplicate inbound is rejected without re-acking") {
    FakeClock clk;
    ReliableEndpoint A(clk), B(clk);
    std::vector<std::uint16_t> acked;
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};

    A.writeOutboundHeader(buf.data(), buf.size());
    PacketHeader h;
    REQUIRE(readPacketHeader(buf.data(), buf.size(), h));
    REQUIRE(B.processInboundHeader(h, acked));

    // Replaying the same header must be rejected.
    CHECK_FALSE(B.processInboundHeader(h, acked));
}

TEST_CASE("ReliableEndpoint smooths RTT over multiple samples") {
    FakeClock clkA, clkB;
    ReliableEndpoint A(clkA), B(clkB);
    std::vector<std::uint16_t> acked;
    std::array<std::uint8_t, kPacketHeaderBytes> buf{};

    auto roundTrip = [&](Clock::Millis ms) {
        A.writeOutboundHeader(buf.data(), buf.size());
        PacketHeader ha;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), ha));
        clkA.advance(ms);
        clkB.advance(ms);
        REQUIRE(B.processInboundHeader(ha, acked));
        B.writeOutboundHeader(buf.data(), buf.size());
        PacketHeader hb;
        REQUIRE(readPacketHeader(buf.data(), buf.size(), hb));
        REQUIRE(A.processInboundHeader(hb, acked));
    };

    roundTrip(50);   // first sample → srtt = 50
    CHECK(A.smoothedRttMs() == doctest::Approx(50.0));
    roundTrip(100);  // srtt = 0.9*50 + 0.1*100 = 55
    CHECK(A.smoothedRttMs() == doctest::Approx(55.0));
    roundTrip(100);  // srtt = 0.9*55 + 0.1*100 = 59.5
    CHECK(A.smoothedRttMs() == doctest::Approx(59.5));
}
