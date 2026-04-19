#include "engine/net/PacketHeader.h"

#include <cstring>

namespace engine::net {

// Big-endian encode/decode helpers. Keeping them local so other TUs
// can't accidentally reach past the header for them.
namespace {

inline void put_u16_be(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<std::uint8_t>(v & 0xFF);
}

inline void put_u32_be(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<std::uint8_t>((v >>  8) & 0xFF);
    p[3] = static_cast<std::uint8_t>(v & 0xFF);
}

inline std::uint16_t get_u16_be(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((std::uint16_t(p[0]) << 8) | p[1]);
}

inline std::uint32_t get_u32_be(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24)
         | (std::uint32_t(p[1]) << 16)
         | (std::uint32_t(p[2]) <<  8)
         |  std::uint32_t(p[3]);
}

static_assert(kPacketHeaderBytes == 12, "header layout assumption");

}  // namespace

bool writePacketHeader(const PacketHeader& h, std::uint8_t* out, std::size_t cap) {
    if (cap < kPacketHeaderBytes || out == nullptr) return false;
    put_u32_be(out + 0,  h.protocolId);
    put_u16_be(out + 4,  h.sequence);
    put_u16_be(out + 6,  h.ack);
    put_u32_be(out + 8,  h.ackBits);
    return true;
}

bool readPacketHeader(const std::uint8_t* in, std::size_t len, PacketHeader& out) {
    if (len < kPacketHeaderBytes || in == nullptr) return false;
    out.protocolId = get_u32_be(in + 0);
    if (out.protocolId != kProtocolId) return false;
    out.sequence = get_u16_be(in + 4);
    out.ack      = get_u16_be(in + 6);
    out.ackBits  = get_u32_be(in + 8);
    return true;
}

}  // namespace engine::net
