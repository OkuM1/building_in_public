#pragma once

// engine::net::Fragmentation — split oversize payloads into MTU-sized
// fragments and reassemble them on the other side. Phase 4d.
//
// Two pieces, both stateless from the outside:
//
//   fragment()   : payload + msg id + max-fragment-bytes → vector of
//                  fragment datagrams, each prefixed with a 6-byte
//                  FragmentHeader.
//
//   Reassembler  : ingest fragments (any order, any timing), emit
//                  fully-reassembled payloads, expire partial sets on
//                  a caller-driven GC sweep so a dropped fragment
//                  doesn't leak memory forever.
//
// Wire header per fragment (separate from, and in addition to, the
// 12-byte packet header from 4b):
//
//   u16 messageId    — unique per fragmented message (caller-assigned)
//   u16 fragIndex    — 0..fragTotal-1
//   u16 fragTotal    — total number of fragments in this message
//
// 6 bytes. Caller appends the fragment payload after this header in
// whatever channel chunk the fragment rides in.
//
// Not in this commit (deferred to 4-closeout):
//   - Plumbing into ReliableChannel.writeInto(). Today the channel
//     refuses messages larger than the per-packet budget; 4d provides
//     the primitive, 4-closeout wires it.
//   - Fragment-level acks. This design relies on the reliable message
//     being acked in aggregate by the channel above — once the full
//     message is delivered, all fragments' packets get acked in the
//     normal ack flow. Until then, any lost fragment retransmits the
//     entire message.
//
// See docs/design/0016-fragmentation.md.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "engine/net/ReliableEndpoint.h"  // for Clock::Millis

namespace engine::net {

using FragmentPayload = std::vector<std::uint8_t>;

// Fixed-size header prefixed to each fragment datagram.
inline constexpr std::size_t kFragmentHeaderBytes = 6;

struct FragmentHeader {
    std::uint16_t messageId = 0;
    std::uint16_t fragIndex = 0;
    std::uint16_t fragTotal = 0;
};

// Safety caps. An entire message can't have more than this many
// fragments; if a caller asks for more, fragment() returns an empty
// vector (the caller should have chunked differently at a higher
// level).
inline constexpr std::uint16_t kMaxFragmentsPerMessage = 256;

// ----------------------------------------------------------------------
// Fragment: split a payload into pieces of at most maxFragmentPayload
// bytes. The returned vector has one entry per fragment; each entry
// begins with a 6-byte FragmentHeader in big-endian.
//
// - `maxFragmentPayload` is the max BODY bytes per fragment (does NOT
//   include the FragmentHeader itself).
// - Returns {} if payload cannot fit in kMaxFragmentsPerMessage
//   fragments or maxFragmentPayload is 0.
// ----------------------------------------------------------------------
std::vector<FragmentPayload> fragment(std::uint16_t messageId,
                                      const std::uint8_t* data,
                                      std::size_t bytes,
                                      std::size_t maxFragmentPayload);

// Header I/O. Big-endian, same discipline as PacketHeader.
bool writeFragmentHeader(const FragmentHeader& h, std::uint8_t* out, std::size_t cap);
bool readFragmentHeader(const std::uint8_t* in, std::size_t len, FragmentHeader& out);

// ----------------------------------------------------------------------
// Reassembler: collect fragments by messageId; emit completed
// payloads; expire incomplete sets on GC.
// ----------------------------------------------------------------------
class Reassembler {
public:
    // Maximum number of in-flight incomplete messages kept in memory.
    // Older entries are evicted (their remaining fragments discarded)
    // when this limit is hit — prevents a malicious peer from spraying
    // fragment 0 of 65535 different message ids and exhausting memory.
    static constexpr std::size_t kMaxInFlight = 64;

    // Default time after which a partial message is dropped even if
    // new fragments are still arriving. A typical session's longest
    // RTT bursts are well under this.
    static constexpr Clock::Millis kDefaultTimeoutMs = 2000;

    // `offer` consumes one fragment datagram (header + body). If this
    // fragment completes a message, the reassembled payload is
    // returned; otherwise std::nullopt.
    //
    // Returns std::nullopt on malformed input (too short, bogus header
    // fields, inconsistent fragTotal for an in-flight message, etc.).
    std::optional<FragmentPayload> offer(const std::uint8_t* data,
                                         std::size_t bytes,
                                         Clock::Millis nowMs);

    // Drop partial messages older than `timeoutMs`.
    void gc(Clock::Millis nowMs, Clock::Millis timeoutMs = kDefaultTimeoutMs);

    std::size_t inFlightCount() const { return inFlight_.size(); }

private:
    struct InFlight {
        std::uint16_t                fragTotal = 0;
        std::uint16_t                received  = 0;  // count of unique fragments
        std::vector<bool>            present;
        std::vector<FragmentPayload> pieces;         // indexed 0..fragTotal-1
        Clock::Millis                firstSeenMs = 0;
    };

    std::unordered_map<std::uint16_t, InFlight> inFlight_;

    void evictOldest();  // when kMaxInFlight is hit
};

}  // namespace engine::net
