#pragma once

// engine::net::ReliableEndpoint — the ack/RTT brain. Phase 4b.
//
// Sits one layer above UdpSocket. Its job:
//
//   1. Tag each outbound packet with a (sequence, ack, ackBits) header.
//   2. Ingest each inbound packet's header, updating:
//        - our record of which sequences we've received (for next send's
//          ack/ackBits),
//        - the list of our own sent sequences the peer just confirmed,
//        - smoothed RTT based on the time between send and ack.
//
// It owns NO socket. Callers feed it bytes in and bytes out; the
// actual UDP I/O is a UdpSocket call. This keeps the class trivially
// testable and keeps one concept per commit — 4b is only about the
// ack/RTT machinery, not about plumbing packets through the engine.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/net/PacketHeader.h"
#include "engine/net/SequenceBuffer.h"

namespace engine::net {

// Minimal monotonic clock abstraction so tests can inject time.
// Milliseconds since some epoch; the value itself doesn't matter,
// only differences do.
struct Clock {
    using Millis = std::uint64_t;
    virtual ~Clock() = default;
    virtual Millis nowMs() const = 0;
};

// Per-sent-packet bookkeeping. Small; 1024 of these is 16 KiB.
struct SentRecord {
    Clock::Millis sentAtMs = 0;
    bool          acked    = false;
};

// Per-received-packet bookkeeping. Exists so we can build ackBits
// accurately even if packets arrive out of order.
struct RecvRecord {
    // no payload needed — presence is the signal for ack building
    bool placeholder = false;
};

class ReliableEndpoint {
public:
    explicit ReliableEndpoint(const Clock& clock) : clock_(&clock) {}

    // --- outbound -------------------------------------------------

    // Returns the sequence number that was assigned to the next
    // packet. Stamps `out` with the full 12-byte header. `cap` must
    // be at least kPacketHeaderBytes.
    std::uint16_t writeOutboundHeader(std::uint8_t* out, std::size_t cap);

    // --- inbound --------------------------------------------------

    // Process an inbound packet header. Appends any of OUR sequences
    // that were newly acked by this packet to `newlyAcked` (cleared
    // first). Returns false if the packet predates our receive window
    // or is a duplicate — caller should drop the payload in that case.
    bool processInboundHeader(const PacketHeader& h,
                              std::vector<std::uint16_t>& newlyAcked);

    // Extended overload that also outputs loss declarations.
    //
    // After resolving acks, any of OUR sent sequences that are more
    // than 32 positions behind the peer's current ack (`h.ack`) and
    // still un-acked in the sent buffer are outside the peer's ack
    // window forever — they will never be acked. Those sequences are
    // appended to `newlyLost` and removed from the sent buffer.
    //
    // Connection uses this to call ReliableChannel::onPacketLost so
    // the reliable channel schedules retransmission.
    bool processInboundHeader(const PacketHeader& h,
                              std::vector<std::uint16_t>& newlyAcked,
                              std::vector<std::uint16_t>& newlyLost);

    // --- queries --------------------------------------------------

    // Smoothed round-trip time in milliseconds. Zero until the first
    // ack has been resolved.
    double smoothedRttMs() const { return srttMs_; }

    std::uint16_t nextOutgoingSequence() const { return nextSeq_; }
    std::uint16_t mostRecentReceivedSequence() const { return mostRecentRecv_; }

private:
    // Resolve acks implied by (ack, ackBits) against our sent buffer.
    void resolveAcks(std::uint16_t ack, std::uint32_t ackBits,
                    std::vector<std::uint16_t>& newlyAcked);

    // Recompute the ackBits we emit based on our received-buffer.
    std::uint32_t computeOutboundAckBits() const;

    const Clock* clock_;

    // Outbound state
    std::uint16_t              nextSeq_ = 0;
    SequenceBuffer<SentRecord> sent_{};

    // Inbound state
    bool                       haveReceivedAny_ = false;
    std::uint16_t              mostRecentRecv_  = 0;
    SequenceBuffer<RecvRecord> recv_{};

    // RTT
    double srttMs_ = 0.0;            // EWMA
    bool   srttInitialised_ = false;

    // EWMA factor: new_srtt = (1 - alpha)*srtt + alpha*sample.
    // Gaffer suggests 0.1 for games; TCP spec uses ~0.125.
    static constexpr double kAlpha = 0.1;
};

// A simple real-time clock for production use. Tests supply their
// own subclass.
class SteadyClock : public Clock {
public:
    Millis nowMs() const override;
};

}  // namespace engine::net
