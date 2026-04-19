#pragma once

// engine::net::PacketHeader — 12-byte fixed prefix on every UDP
// datagram the engine emits. Phase 4b.
//
// Layout (big-endian on the wire; we are explicit about byte order so
// client and server can differ in endianness without surprises):
//
//   offset  size  field          purpose
//   0       4     protocolId     magic; drop foreign/old-version packets
//   4       2     sequence       monotonically-increasing packet number
//   6       2     ack            peer's most recent sequence we got
//   8       4     ackBits        bitfield of 32 preceding acked seqs
//
// Total: 12 bytes. Channel id + flags are Phase 4c and will extend
// this struct; the on-wire offsets above must stay stable.
//
// Sequence numbers are 16-bit and wrap. Compare with seqGreater(), not <.

#include <cstddef>
#include <cstdint>

namespace engine::net {

// Magic value prefixed to every packet. Changes whenever the wire
// format of the header itself changes. Phase 4b = value 1.
inline constexpr std::uint32_t kProtocolId = 0x4D45'4E47u;  // "MENG"

// Fixed header size. asserted in the .cpp against (de)serializer.
inline constexpr std::size_t kPacketHeaderBytes = 12;

struct PacketHeader {
    std::uint32_t protocolId = kProtocolId;
    std::uint16_t sequence   = 0;
    std::uint16_t ack        = 0;
    std::uint32_t ackBits    = 0;
};

// Wire I/O. Both are branchless big-endian pack/unpack — no host-order
// assumption. Returns false on buffer-too-small or protocol mismatch.
bool writePacketHeader(const PacketHeader& h, std::uint8_t* out, std::size_t cap);
bool readPacketHeader(const std::uint8_t* in, std::size_t len, PacketHeader& out);

// Sequence-number arithmetic that respects 16-bit wrap.
// True iff `a` is "newer" than `b` assuming the gap is < 2^15.
inline bool seqGreater(std::uint16_t a, std::uint16_t b) {
    // Classic RFC-1982 comparison, specialised to u16. Cast to
    // unsigned explicitly — otherwise u16 promotes to int and the
    // subtraction's signedness trips -Wsign-compare.
    const std::uint32_t ua = a;
    const std::uint32_t ub = b;
    return ((ua > ub) && (ua - ub <= 0x8000u))
        || ((ua < ub) && (ub - ua >  0x8000u));
}

}  // namespace engine::net
