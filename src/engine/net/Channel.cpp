#include "engine/net/Channel.h"

#include <utility>

namespace engine::net {

namespace {

// Serialise a byte payload after a varint length. Payload bytes are
// byte-aligned; no bit-level packing. Channels are coarse-grained.
void writePayload(BitWriter& out, const std::uint8_t* data, std::size_t n) {
    out.writeVarint(static_cast<std::uint32_t>(n));
    for (std::size_t i = 0; i < n; ++i) out.writeUint8(data[i]);
}

bool readPayload(BitReader& in, Payload& out) {
    const std::uint32_t n = in.readVarint();
    if (!in.ok()) return false;
    out.resize(n);
    for (std::uint32_t i = 0; i < n; ++i) out[i] = in.readUint8();
    return in.ok();
}

}  // namespace

// ---------------------------------------------------------------------------
// UnreliableChannel
// ---------------------------------------------------------------------------

void UnreliableChannel::send(const std::uint8_t* data, std::size_t n) {
    outbox_.emplace_back(data, data + n);
}

std::size_t UnreliableChannel::writeInto(BitWriter& out) {
    const std::size_t count = outbox_.size();
    out.writeVarint(static_cast<std::uint32_t>(count));
    for (const auto& p : outbox_) writePayload(out, p.data(), p.size());
    outbox_.clear();
    return count;
}

bool UnreliableChannel::readFrom(BitReader& in) {
    const std::uint32_t count = in.readVarint();
    if (!in.ok()) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        Payload p;
        if (!readPayload(in, p)) return false;
        inbox_.emplace_back(std::move(p));
    }
    return true;
}

std::optional<Payload> UnreliableChannel::receive() {
    if (inbox_.empty()) return std::nullopt;
    Payload p = std::move(inbox_.front());
    inbox_.pop_front();
    return p;
}

// ---------------------------------------------------------------------------
// ReliableChannel — Sender
// ---------------------------------------------------------------------------

std::uint16_t ReliableChannel::send(const std::uint8_t* data, std::size_t n) {
    OutMessage m;
    m.id      = nextSendId_++;
    m.payload.assign(data, data + n);
    outbox_.push_back(std::move(m));
    return outbox_.back().id;
}

std::size_t ReliableChannel::writeInto(BitWriter& out, std::uint16_t packetSeq) {
    // First count how many we will bundle. A two-pass walk keeps the
    // wire-format varint count honest.
    std::size_t count = 0;
    for (auto& m : outbox_) {
        if (!m.inFlightInPacket.has_value()) ++count;
    }

    out.writeVarint(static_cast<std::uint32_t>(count));
    if (count == 0) return 0;

    for (auto& m : outbox_) {
        if (m.inFlightInPacket.has_value()) continue;
        out.writeUint16(m.id);
        writePayload(out, m.payload.data(), m.payload.size());
        m.inFlightInPacket = packetSeq;
    }
    return count;
}

void ReliableChannel::onPacketAcked(std::uint16_t packetSeq) {
    // Remove acked messages in place.
    for (auto it = outbox_.begin(); it != outbox_.end(); ) {
        if (it->inFlightInPacket && *it->inFlightInPacket == packetSeq) {
            it = outbox_.erase(it);
        } else {
            ++it;
        }
    }
}

void ReliableChannel::onPacketLost(std::uint16_t packetSeq) {
    for (auto& m : outbox_) {
        if (m.inFlightInPacket && *m.inFlightInPacket == packetSeq) {
            m.inFlightInPacket.reset();  // available for retransmit
        }
    }
}

// ---------------------------------------------------------------------------
// ReliableChannel — Receiver
// ---------------------------------------------------------------------------

bool ReliableChannel::readFrom(BitReader& in) {
    const std::uint32_t count = in.readVarint();
    if (!in.ok()) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint16_t id = in.readUint16();
        Payload p;
        if (!readPayload(in, p)) return false;

        if (deliveredIds_.count(id)) continue;  // duplicate
        if (holdback_.count(id))     continue;  // duplicate (held for ordering)

        if (!inOrder_) {
            deliveredIds_[id] = true;
            deliver(std::move(p));
            continue;
        }

        // Ordered mode: deliver now iff this is the next expected id,
        // otherwise hold back.
        if (id == expectedRecvId_) {
            deliveredIds_[id] = true;
            deliver(std::move(p));
            expectedRecvId_ = static_cast<std::uint16_t>(expectedRecvId_ + 1);

            // Drain any contiguous run from holdback.
            auto it = holdback_.find(expectedRecvId_);
            while (it != holdback_.end()) {
                deliveredIds_[it->first] = true;
                deliver(std::move(it->second));
                holdback_.erase(it);
                expectedRecvId_ = static_cast<std::uint16_t>(expectedRecvId_ + 1);
                it = holdback_.find(expectedRecvId_);
            }
        } else {
            // Ignore messages that are older than we've already advanced
            // past — they're duplicates of long-acked traffic.
            const std::uint16_t gap = static_cast<std::uint16_t>(id - expectedRecvId_);
            if (gap >= 0x8000u) continue;  // id < expectedRecvId_ (wrap-aware)
            holdback_.emplace(id, std::move(p));
        }
    }
    return true;
}

std::optional<Payload> ReliableChannel::receive() {
    if (ready_.empty()) return std::nullopt;
    Payload p = std::move(ready_.front());
    ready_.pop_front();
    return p;
}

void ReliableChannel::deliver(Payload p) {
    ready_.emplace_back(std::move(p));
}

}  // namespace engine::net
