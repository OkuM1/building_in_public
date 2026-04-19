#pragma once

// engine::net::*Channel — the three delivery flavours the engine
// needs to carry application-level messages on top of the packet
// ack/RTT machinery from Phase 4b.
//
// Phase 4c. Three channels:
//
//   UnreliableChannel       — snapshot traffic. No ids, no retransmit.
//                             Newer supersedes older; drops are absorbed
//                             by the *next* snapshot.
//
//   ReliableChannel         — eventful messages ("you were eliminated",
//   (in-order or not)        "round started"). Sender keeps a copy until
//                             the containing packet is acked; retransmits
//                             on loss. Receiver dedupes by message id.
//                             When constructed with inOrder=true, the
//                             receiver holds out-of-order messages until
//                             the gap fills before delivering.
//
// Channels own NO socket and NO endpoint. They read/write their own
// section of a BitStream. This keeps one concept per commit and lets
// tests exercise the delivery semantics without involving the network.
//
// Wire format (for a single channel chunk inside a packet):
//
//   Unreliable:   varint count | { varint payload_len, bytes }*
//   Reliable:     varint count | { u16 msg_id, varint payload_len, bytes }*
//
// Packet-level framing (which channel chunk comes first, channel id,
// etc.) is Phase 4d material. For 4c, the caller is responsible for
// pairing an outbound channel with its peer inbound channel.
//
// See docs/design/0015-channels.md.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "engine/net/BitStream.h"

namespace engine::net {

using Payload = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------
// UnreliableChannel
// ---------------------------------------------------------------------------
//
// Trivial: send() pushes a payload into the outbox; writeInto() drains
// the outbox verbatim into a BitStream; readFrom() pushes parsed
// payloads into the inbox; receive() pops from the inbox.
//
// A payload that didn't fit in the packet because the caller stopped
// draining the outbox stays in the outbox. Phase 4d formalises MTU
// enforcement; today writeInto writes everything.
class UnreliableChannel {
public:
    void send(const std::uint8_t* data, std::size_t n);
    void send(const Payload& p) { send(p.data(), p.size()); }

    // Drain outbox into `out`. Returns number of messages written.
    std::size_t writeInto(BitWriter& out);

    // Parse inbound messages from `in` and queue for receive().
    bool readFrom(BitReader& in);

    std::optional<Payload> receive();
    std::size_t            outboxSize() const { return outbox_.size(); }
    std::size_t            inboxSize() const { return inbox_.size(); }

private:
    std::deque<Payload> outbox_;
    std::deque<Payload> inbox_;
};

// ---------------------------------------------------------------------------
// ReliableChannel
// ---------------------------------------------------------------------------
//
// One instance covers both reliable-unordered (default) and
// reliable-ordered (pass `inOrder=true` to the ctor). The only
// behavioural difference is the receive side: ordered mode holds a
// message back until all lower message ids have been delivered.
//
// Sender model
// ------------
// Every send() gets a 16-bit message id (sequential, wraps). A
// message stays in the outbox with state (inFlightInPacket) until the
// packet it was last included in is acked. Per writeInto(packetSeq):
//
//   - Walk the outbox in id order.
//   - For messages whose inFlightInPacket is std::nullopt (never sent
//     or previously declared lost), include them, set their
//     inFlightInPacket = packetSeq.
//
// Per onPacketAcked(seq):
//   - Remove all outbox messages whose inFlightInPacket == seq.
//
// Per onPacketLost(seq):
//   - Reset inFlightInPacket = std::nullopt on those messages so the
//     next writeInto picks them up. (Loss detection is caller-supplied
//     in Phase 4c; 4b will wire this from ReliableEndpoint.)
//
// Receiver model
// --------------
// readFrom() parses (msg_id, payload) pairs. Duplicates (ids already
// seen) are dropped. New ids are routed:
//   - Unordered: delivered immediately into the ready queue.
//   - Ordered:   if msg_id == expectedRecvId_, deliver and advance;
//                else buffer in the holdback map keyed by id.
//                After each delivery, drain the holdback head-first
//                for any contiguous ids.
class ReliableChannel {
public:
    explicit ReliableChannel(bool inOrder = false) : inOrder_(inOrder) {}

    // --- Sender -------------------------------------------------------

    // Enqueues `payload`, assigns it a new message id, returns the id.
    std::uint16_t send(const std::uint8_t* data, std::size_t n);
    std::uint16_t send(const Payload& p) { return send(p.data(), p.size()); }

    // Serialise as many currently-unflight outbox messages as fit
    // (for Phase 4c: all of them) into `out`, recording which ids
    // were bundled into `packetSeq`. Returns count written.
    std::size_t writeInto(BitWriter& out, std::uint16_t packetSeq);

    // Called by the outer layer when a packet is confirmed delivered.
    void onPacketAcked(std::uint16_t packetSeq);

    // Called by the outer layer when a packet has been declared lost.
    void onPacketLost(std::uint16_t packetSeq);

    std::size_t outboxSize() const { return outbox_.size(); }

    // --- Receiver -----------------------------------------------------

    bool readFrom(BitReader& in);
    std::optional<Payload> receive();
    std::size_t            inboxSize() const { return ready_.size(); }

private:
    struct OutMessage {
        std::uint16_t id;
        Payload       payload;
        std::optional<std::uint16_t> inFlightInPacket;
    };

    // Sender state
    std::deque<OutMessage> outbox_;
    std::uint16_t          nextSendId_ = 0;

    // Receiver state
    bool                                 inOrder_ = false;
    std::uint16_t                        expectedRecvId_ = 0;
    std::map<std::uint16_t, Payload>     holdback_;    // ordered mode only
    // The ordered receiver also accepts ids older than expectedRecvId_
    // within a small window as duplicates. We track the most recent N
    // delivered ids for dedup in unordered mode.
    std::unordered_map<std::uint16_t, bool> deliveredIds_;
    std::deque<Payload>                  ready_;

    void deliver(Payload p);
};

}  // namespace engine::net
