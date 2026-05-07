#pragma once

// engine::net::Connection — per-peer UDP facade. Phase 4 closeout.
//
// Wires together the five Phase 4 sub-commits into one coherent API:
//
//   ReliableEndpoint       (ack + RTT tracking,   Phase 4b)
//   UnreliableChannel      (snapshots,             Phase 4c)
//   ReliableChannel ×2     (events + lobby,        Phase 4c)
//   CongestionController   (send-rate gating,      Phase 4e)
//
// Each Connection is one side of a peer-to-peer channel. For a
// client/server topology: the server holds one Connection per
// connected client; each client holds one Connection to the server.
//
// Packet wire format
// ------------------
// Every packet is a flat byte buffer:
//
//   [ 12 bytes PacketHeader        ]   ← Phase 4b
//   [ channel-0 section (snapshot) ]   ← UnreliableChannel
//   [ channel-1 section (events)   ]   ← ReliableChannel, unordered
//   [ channel-2 section (lobby)    ]   ← ReliableChannel, ordered
//
// Each channel section is self-framing via a leading varint message
// count (from Channel::writeInto / readFrom). The receiver reads
// exactly that many messages then moves to the next section.
//
// Loss detection
// --------------
// processInboundHeader (extended in Phase 4 closeout) outputs both
// newlyAcked and newlyLost packet sequences. Connection forwards both
// to the two reliable channels so retransmission triggers correctly.
//
// See docs/design/0019-connection-glue.md.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/net/Channel.h"
#include "engine/net/CongestionController.h"
#include "engine/net/ReliableEndpoint.h"

namespace engine::net {

// Which of the three channels to use for a given message.
enum class ChannelId : std::uint8_t {
    Unreliable        = 0,  // snapshots — fire-and-forget
    ReliableUnordered = 1,  // events (eliminations, round start)
    ReliableOrdered   = 2,  // lobby state — ordered, exactly-once
};

class Connection {
public:
    explicit Connection(const Clock&     clock,
                        CongestionConfig congCfg = {});

    // ----- Sending ---------------------------------------------------

    // Enqueue a message for the given channel.
    void send(ChannelId ch, const std::uint8_t* data, std::size_t n);
    void send(ChannelId ch, const Payload& p) { send(ch, p.data(), p.size()); }

    // True when the congestion controller allows a send right now.
    // Callers should check this before calling buildPacket() to avoid
    // overdriving the link.
    bool shouldSendNow(Clock::Millis nowMs) const;

    // Build one outbound packet (header + all three channel sections).
    // The caller transmits the returned bytes.
    // Updates the internal next-send timer; check shouldSendNow first.
    //
    // Always produces a valid packet even when all channels are empty:
    // the header carries acks back to the peer so their loss-detection
    // can advance.
    std::vector<std::uint8_t> buildPacket(Clock::Millis nowMs);

    // ----- Receiving -------------------------------------------------

    // Process one inbound packet. Returns false if the packet is
    // malformed, a protocol-id mismatch, or a duplicate sequence.
    // On success: updates RTT + congestion state, notifies channels of
    // acked / lost packet sequences, and queues payloads for receive().
    bool receivePacket(const std::uint8_t* data, std::size_t n,
                       Clock::Millis nowMs);

    // Pop the next delivered message from the given channel.
    std::optional<Payload> receive(ChannelId ch);

    // ----- Queries ---------------------------------------------------

    // Smoothed RTT in milliseconds. Zero until the first ack resolves.
    double smoothedRttMs() const { return endpoint_.smoothedRttMs(); }

    // Current send-rate mode from the congestion controller.
    CongestionMode congestionMode() const { return congCtrl_.mode(); }

private:
    ReliableEndpoint     endpoint_;
    UnreliableChannel    ch0_;              // channel 0 — unreliable
    ReliableChannel      ch1_;             // channel 1 — reliable, unordered
    ReliableChannel      ch2_;             // channel 2 — reliable, ordered
    CongestionController congCtrl_;
    Clock::Millis        nextSendMs_ = 0;  // earliest time we may send next
};

}  // namespace engine::net
