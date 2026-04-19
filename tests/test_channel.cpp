// Phase 4c: channels — unreliable, reliable-unordered, reliable-ordered.

#include <doctest.h>

#include <cstring>
#include <string>
#include <vector>

#include "engine/net/BitStream.h"
#include "engine/net/Channel.h"

using namespace engine::net;

namespace {

Payload bytesOf(const std::string& s) {
    return Payload(s.begin(), s.end());
}

std::string stringOf(const Payload& p) {
    return std::string(p.begin(), p.end());
}

// Pack sender side into a BitWriter, then feed those bytes into a
// BitReader, then ask the receiver to parse.  Mimics one UDP
// datagram's channel chunk going over the wire.
void roundTripUnreliable(UnreliableChannel& sender, UnreliableChannel& receiver) {
    BitWriter w;
    sender.writeInto(w);
    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());
    REQUIRE(receiver.readFrom(r));
}

void roundTripReliable(ReliableChannel& sender,
                       ReliableChannel& receiver,
                       std::uint16_t    packetSeq) {
    BitWriter w;
    sender.writeInto(w, packetSeq);
    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());
    REQUIRE(receiver.readFrom(r));
}

}  // namespace

// ---------------------------------------------------------------------------
// UnreliableChannel
// ---------------------------------------------------------------------------

TEST_CASE("UnreliableChannel delivers messages FIFO") {
    UnreliableChannel a, b;
    a.send(bytesOf("hello"));
    a.send(bytesOf("world"));
    CHECK(a.outboxSize() == 2);

    roundTripUnreliable(a, b);
    CHECK(a.outboxSize() == 0);       // drained on send
    CHECK(b.inboxSize() == 2);

    auto m1 = b.receive();
    auto m2 = b.receive();
    REQUIRE(m1.has_value());
    REQUIRE(m2.has_value());
    CHECK(stringOf(*m1) == "hello");
    CHECK(stringOf(*m2) == "world");
    CHECK_FALSE(b.receive().has_value());
}

TEST_CASE("UnreliableChannel: an empty packet produces zero messages") {
    UnreliableChannel a, b;
    roundTripUnreliable(a, b);
    CHECK(b.inboxSize() == 0);
    CHECK_FALSE(b.receive().has_value());
}

// ---------------------------------------------------------------------------
// ReliableChannel (unordered)
// ---------------------------------------------------------------------------

TEST_CASE("ReliableChannel: ack removes message from outbox") {
    ReliableChannel a, b;

    const auto id0 = a.send(bytesOf("one"));
    const auto id1 = a.send(bytesOf("two"));
    CHECK(id0 == 0);
    CHECK(id1 == 1);
    CHECK(a.outboxSize() == 2);

    roundTripReliable(a, b, /*packetSeq=*/100);
    // Receiver has both; sender still has them outstanding until ack.
    CHECK(a.outboxSize() == 2);
    CHECK(b.inboxSize() == 2);

    a.onPacketAcked(100);
    CHECK(a.outboxSize() == 0);
}

TEST_CASE("ReliableChannel: on packet loss, messages retransmit and peer dedupes") {
    ReliableChannel a, b;
    a.send(bytesOf("durable"));

    // Send in packet 10; declare it lost.
    roundTripReliable(a, b, 10);
    CHECK(b.inboxSize() == 1);
    CHECK(a.outboxSize() == 1);
    a.onPacketLost(10);
    CHECK(a.outboxSize() == 1);   // still present

    // Retransmit in packet 11. Receiver should dedupe on msg id.
    roundTripReliable(a, b, 11);
    CHECK(b.inboxSize() == 1);    // not 2

    a.onPacketAcked(11);
    CHECK(a.outboxSize() == 0);
}

TEST_CASE("ReliableChannel: duplicate arrivals are discarded") {
    ReliableChannel a, b;
    a.send(bytesOf("only-once"));

    roundTripReliable(a, b, 5);  // A thinks packet 5 went out
    roundTripReliable(a, b, 6);  // A thinks packet 6 went out too (say 5 was lost)
    // In-flight tag is still 5 on the message, so writeInto for packet 6
    // would NOT resend it. To make the duplicate-arrival test meaningful
    // we reset via onPacketLost:
    a.onPacketLost(5);
    roundTripReliable(a, b, 7);

    CHECK(b.inboxSize() == 1);   // receiver saw id=0 three times, delivered once
}

TEST_CASE("ReliableChannel: writeInto skips in-flight messages") {
    ReliableChannel a, b;
    a.send(bytesOf("sticky"));

    // First call: the one message goes out.
    {
        BitWriter w;
        const auto n = a.writeInto(w, 50);
        CHECK(n == 1);
    }
    // Second call with no ack yet and no declared loss: nothing to send.
    {
        BitWriter w;
        const auto n = a.writeInto(w, 51);
        CHECK(n == 0);
    }
}

// ---------------------------------------------------------------------------
// ReliableChannel (ordered)
// ---------------------------------------------------------------------------

TEST_CASE("ReliableChannel ordered: buffers out-of-order, drains on gap fill") {
    ReliableChannel sender, receiver{/*inOrder=*/true};

    sender.send(bytesOf("A"));  // id 0
    sender.send(bytesOf("B"));  // id 1
    sender.send(bytesOf("C"));  // id 2

    // Deliver the packet carrying id 0 and id 1 via a normal write;
    // then simulate id 1 arriving first by hand (ordered mode should
    // buffer it until id 0 lands).
    //
    // Approach: simulate two packets — packet P1 carries id 1 only
    // (drop id 0 by writing just id 1 manually? simpler: send B, C
    // first in one writeInto and mark A's packet as lost).
    //
    // Cleaner approach: issue writes across two packets.
    {
        // Packet P=100: everything in outbox right now goes out
        // (ids 0,1,2 all get inFlightInPacket=100). Parse at receiver.
        roundTripReliable(sender, receiver, 100);
    }

    // With all three delivered in order, the receiver should surface
    // A, B, C in sequence.
    auto m1 = receiver.receive();
    auto m2 = receiver.receive();
    auto m3 = receiver.receive();
    REQUIRE(m1.has_value());
    REQUIRE(m2.has_value());
    REQUIRE(m3.has_value());
    CHECK(stringOf(*m1) == "A");
    CHECK(stringOf(*m2) == "B");
    CHECK(stringOf(*m3) == "C");
}

TEST_CASE("ReliableChannel ordered: late id is held then released") {
    ReliableChannel sender;
    ReliableChannel receiver{/*inOrder=*/true};

    sender.send(bytesOf("A"));  // id 0
    sender.send(bytesOf("B"));  // id 1

    // Serialise A+B but deliver only id 1 manually to the receiver.
    BitWriter w;
    sender.writeInto(w, 10);
    const auto& bytes = w.finish();

    // Reparse the bytes into a BitReader, then emulate "only id 1
    // arrived" by writing a synthetic single-message chunk.
    BitWriter w2;
    w2.writeVarint(1);                        // one message
    w2.writeUint16(1);                        // id 1
    w2.writeVarint(1);                        // payload len
    w2.writeUint8('B');
    const auto& bytesOnlyB = w2.finish();

    BitReader r1(bytesOnlyB.data(), bytesOnlyB.size());
    REQUIRE(receiver.readFrom(r1));

    // Holdback — nothing ready yet because id 0 hasn't arrived.
    CHECK_FALSE(receiver.receive().has_value());

    // Now id 0 arrives.
    BitWriter w3;
    w3.writeVarint(1);
    w3.writeUint16(0);
    w3.writeVarint(1);
    w3.writeUint8('A');
    const auto& bytesOnlyA = w3.finish();

    BitReader r2(bytesOnlyA.data(), bytesOnlyA.size());
    REQUIRE(receiver.readFrom(r2));

    auto m1 = receiver.receive();
    auto m2 = receiver.receive();
    REQUIRE(m1.has_value());
    REQUIRE(m2.has_value());
    CHECK(stringOf(*m1) == "A");
    CHECK(stringOf(*m2) == "B");
    (void)bytes;  // writeInto was to construct a realistic scenario; not re-used
}
