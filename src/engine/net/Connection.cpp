#include "engine/net/Connection.h"

#include <cstring>

#include "engine/net/BitStream.h"
#include "engine/net/PacketHeader.h"

namespace engine::net {

Connection::Connection(const Clock& clock, CongestionConfig congCfg)
    : endpoint_(clock)
    , ch1_(/*inOrder=*/false)
    , ch2_(/*inOrder=*/true)
    , congCtrl_(congCfg)
{}

// ---------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------

void Connection::send(ChannelId ch, const std::uint8_t* data, std::size_t n) {
    switch (ch) {
    case ChannelId::Unreliable:
        ch0_.send(data, n);
        break;
    case ChannelId::ReliableUnordered:
        ch1_.send(data, n);
        break;
    case ChannelId::ReliableOrdered:
        ch2_.send(data, n);
        break;
    }
}

bool Connection::shouldSendNow(Clock::Millis nowMs) const {
    return nowMs >= nextSendMs_;
}

std::vector<std::uint8_t> Connection::buildPacket(Clock::Millis nowMs) {
    // 1. Stamp the 12-byte packet header; capture the outgoing sequence
    //    number so the reliable channels can record which messages rode
    //    in this packet.
    std::uint8_t headerBuf[kPacketHeaderBytes]{};
    const std::uint16_t seq = endpoint_.writeOutboundHeader(headerBuf,
                                                            kPacketHeaderBytes);

    // 2. Write all three channel sections into a single BitWriter.
    //    Each section starts with a varint message count so the receiver
    //    can parse them back-to-back without inter-section delimiters.
    BitWriter bw;
    ch0_.writeInto(bw);
    ch1_.writeInto(bw, seq);
    ch2_.writeInto(bw, seq);
    const auto& channelBytes = bw.finish();

    // 3. Concatenate header + channel data.
    std::vector<std::uint8_t> pkt(kPacketHeaderBytes + channelBytes.size());
    std::memcpy(pkt.data(), headerBuf, kPacketHeaderBytes);
    std::memcpy(pkt.data() + kPacketHeaderBytes,
                channelBytes.data(), channelBytes.size());

    // 4. Advance the next-send window.
    const auto intervalMs = static_cast<Clock::Millis>(
        congCtrl_.millisBetweenSends());
    nextSendMs_ = nowMs + intervalMs;

    return pkt;
}

// ---------------------------------------------------------------------------
// Receiving
// ---------------------------------------------------------------------------

bool Connection::receivePacket(const std::uint8_t* data, std::size_t n,
                               Clock::Millis nowMs) {
    if (n < kPacketHeaderBytes) return false;

    // 1. Parse and validate the 12-byte header.
    PacketHeader h;
    if (!readPacketHeader(data, n, h)) return false;

    // 2. Feed to ReliableEndpoint; collect acked and lost packet seqs.
    std::vector<std::uint16_t> newlyAcked;
    std::vector<std::uint16_t> newlyLost;
    if (!endpoint_.processInboundHeader(h, newlyAcked, newlyLost)) return false;

    // 3. Notify reliable channels so they can remove acked messages and
    //    schedule retransmission of messages in lost packets.
    for (std::uint16_t seq : newlyAcked) {
        ch1_.onPacketAcked(seq);
        ch2_.onPacketAcked(seq);
    }
    for (std::uint16_t seq : newlyLost) {
        ch1_.onPacketLost(seq);
        ch2_.onPacketLost(seq);
    }

    // 4. Update the congestion controller now that we have a fresh RTT.
    congCtrl_.update(endpoint_.smoothedRttMs(), nowMs);

    // 5. Decode the channel sections from the remainder of the packet.
    const std::uint8_t* payload = data + kPacketHeaderBytes;
    const std::size_t   payLen  = n - kPacketHeaderBytes;
    BitReader br(payload, payLen);
    if (!ch0_.readFrom(br)) return false;
    if (!ch1_.readFrom(br)) return false;
    if (!ch2_.readFrom(br)) return false;

    return true;
}

std::optional<Payload> Connection::receive(ChannelId ch) {
    switch (ch) {
    case ChannelId::Unreliable:         return ch0_.receive();
    case ChannelId::ReliableUnordered:  return ch1_.receive();
    case ChannelId::ReliableOrdered:    return ch2_.receive();
    }
    return std::nullopt;
}

}  // namespace engine::net
