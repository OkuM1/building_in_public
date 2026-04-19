#include "engine/net/ReliableEndpoint.h"

#include <chrono>

namespace engine::net {

// ---------------------------------------------------------------------------
// SteadyClock
// ---------------------------------------------------------------------------

Clock::Millis SteadyClock::nowMs() const {
    using namespace std::chrono;
    return static_cast<Millis>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---------------------------------------------------------------------------
// ReliableEndpoint
// ---------------------------------------------------------------------------

std::uint16_t ReliableEndpoint::writeOutboundHeader(std::uint8_t* out, std::size_t cap) {
    const std::uint16_t seq = nextSeq_++;

    // Record this send so a later ack can yield an RTT sample.
    SentRecord& rec = sent_.insert(seq);
    rec.sentAtMs = clock_->nowMs();
    rec.acked    = false;

    PacketHeader h;
    h.protocolId = kProtocolId;
    h.sequence   = seq;
    h.ack        = haveReceivedAny_ ? mostRecentRecv_ : std::uint16_t{0};
    h.ackBits    = computeOutboundAckBits();

    writePacketHeader(h, out, cap);
    return seq;
}

bool ReliableEndpoint::processInboundHeader(const PacketHeader& h,
                                            std::vector<std::uint16_t>& newlyAcked) {
    newlyAcked.clear();

    // Update our receive window so future outbound ackBits reflect
    // this packet.
    if (!haveReceivedAny_) {
        haveReceivedAny_ = true;
        mostRecentRecv_  = h.sequence;
        recv_.insert(h.sequence);
    } else if (seqGreater(h.sequence, mostRecentRecv_)) {
        // New packets beyond mostRecentRecv_: drop stale entries we're
        // rolling past. The SequenceBuffer overwrites on insert, so
        // we don't explicitly erase — but we DO need to scrub any
        // slots in the gap that happen to collide with old sequences
        // we never saw, otherwise computeOutboundAckBits could report
        // a spurious 1.
        std::uint16_t s = static_cast<std::uint16_t>(mostRecentRecv_ + 1);
        while (seqGreater(h.sequence, s)) {
            recv_.remove(s);
            s = static_cast<std::uint16_t>(s + 1);
        }
        mostRecentRecv_ = h.sequence;
        recv_.insert(h.sequence);
    } else if (h.sequence == mostRecentRecv_) {
        // Duplicate of our most recent — drop.
        return false;
    } else {
        // Out-of-order but within window.
        if (recv_.exists(h.sequence)) {
            return false;  // duplicate
        }
        // Too old to fit in our 32-bit ack window? Drop — can't ack it.
        const std::uint16_t gap = static_cast<std::uint16_t>(mostRecentRecv_ - h.sequence);
        if (gap >= 32) return false;
        recv_.insert(h.sequence);
    }

    // Resolve acks carried BY this packet against our sent buffer.
    resolveAcks(h.ack, h.ackBits, newlyAcked);
    return true;
}

void ReliableEndpoint::resolveAcks(std::uint16_t ack, std::uint32_t ackBits,
                                   std::vector<std::uint16_t>& newlyAcked) {
    // ack itself, then 32 preceding bits. Bit i = ack - 1 - i.
    const Clock::Millis now = clock_->nowMs();

    auto tryResolve = [&](std::uint16_t seq) {
        SentRecord* rec = sent_.find(seq);
        if (!rec || rec->acked) return;
        rec->acked = true;
        newlyAcked.push_back(seq);

        // RTT sample.
        const double sample = static_cast<double>(now - rec->sentAtMs);
        if (!srttInitialised_) {
            srttMs_ = sample;
            srttInitialised_ = true;
        } else {
            srttMs_ = (1.0 - kAlpha) * srttMs_ + kAlpha * sample;
        }
    };

    tryResolve(ack);
    for (int i = 0; i < 32; ++i) {
        if (ackBits & (1u << i)) {
            tryResolve(static_cast<std::uint16_t>(ack - 1 - i));
        }
    }
}

std::uint32_t ReliableEndpoint::computeOutboundAckBits() const {
    if (!haveReceivedAny_) return 0;
    std::uint32_t bits = 0;
    for (int i = 0; i < 32; ++i) {
        const std::uint16_t seq = static_cast<std::uint16_t>(mostRecentRecv_ - 1 - i);
        // const_cast-free const lookup: we only need presence.
        SequenceBuffer<RecvRecord>& nc = const_cast<SequenceBuffer<RecvRecord>&>(recv_);
        if (nc.exists(seq)) bits |= (1u << i);
    }
    return bits;
}

}  // namespace engine::net
