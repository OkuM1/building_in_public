#include "engine/net/Fragmentation.h"

#include <algorithm>
#include <cstring>

namespace engine::net {

namespace {

inline void put_u16_be(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<std::uint8_t>(v & 0xFF);
}

inline std::uint16_t get_u16_be(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((std::uint16_t(p[0]) << 8) | p[1]);
}

}  // namespace

bool writeFragmentHeader(const FragmentHeader& h, std::uint8_t* out, std::size_t cap) {
    if (cap < kFragmentHeaderBytes || out == nullptr) return false;
    put_u16_be(out + 0, h.messageId);
    put_u16_be(out + 2, h.fragIndex);
    put_u16_be(out + 4, h.fragTotal);
    return true;
}

bool readFragmentHeader(const std::uint8_t* in, std::size_t len, FragmentHeader& out) {
    if (len < kFragmentHeaderBytes || in == nullptr) return false;
    out.messageId = get_u16_be(in + 0);
    out.fragIndex = get_u16_be(in + 2);
    out.fragTotal = get_u16_be(in + 4);
    return true;
}

std::vector<FragmentPayload> fragment(std::uint16_t messageId,
                                      const std::uint8_t* data,
                                      std::size_t bytes,
                                      std::size_t maxFragmentPayload) {
    if (maxFragmentPayload == 0) return {};

    // Ceiling division; bytes==0 still produces one empty fragment so
    // the receiver can surface zero-length messages.
    const std::size_t fragCount = bytes == 0
        ? 1u
        : (bytes + maxFragmentPayload - 1) / maxFragmentPayload;
    if (fragCount > kMaxFragmentsPerMessage) return {};

    std::vector<FragmentPayload> out;
    out.reserve(fragCount);

    for (std::size_t i = 0; i < fragCount; ++i) {
        const std::size_t off  = i * maxFragmentPayload;
        const std::size_t take = std::min(maxFragmentPayload, bytes - off);

        FragmentPayload frag;
        frag.resize(kFragmentHeaderBytes + take);

        FragmentHeader h;
        h.messageId = messageId;
        h.fragIndex = static_cast<std::uint16_t>(i);
        h.fragTotal = static_cast<std::uint16_t>(fragCount);
        writeFragmentHeader(h, frag.data(), frag.size());

        if (take > 0) {
            std::memcpy(frag.data() + kFragmentHeaderBytes, data + off, take);
        }
        out.emplace_back(std::move(frag));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Reassembler
// ---------------------------------------------------------------------------

std::optional<FragmentPayload> Reassembler::offer(const std::uint8_t* data,
                                                   std::size_t bytes,
                                                   Clock::Millis nowMs) {
    FragmentHeader h;
    if (!readFragmentHeader(data, bytes, h)) return std::nullopt;
    if (h.fragTotal == 0 || h.fragTotal > kMaxFragmentsPerMessage) return std::nullopt;
    if (h.fragIndex >= h.fragTotal) return std::nullopt;

    const std::uint8_t* body      = data + kFragmentHeaderBytes;
    const std::size_t   bodyBytes = bytes - kFragmentHeaderBytes;

    auto it = inFlight_.find(h.messageId);
    if (it == inFlight_.end()) {
        // Fast path: single-fragment message, no state needed.
        if (h.fragTotal == 1) {
            FragmentPayload out(body, body + bodyBytes);
            return out;
        }

        if (inFlight_.size() >= kMaxInFlight) evictOldest();

        InFlight f;
        f.fragTotal   = h.fragTotal;
        f.firstSeenMs = nowMs;
        f.present.assign(h.fragTotal, false);
        f.pieces.resize(h.fragTotal);
        auto inserted = inFlight_.emplace(h.messageId, std::move(f));
        it = inserted.first;
    }

    InFlight& f = it->second;

    // Sanity: fragTotal must be consistent across all pieces of a
    // message. Disagreement means we're seeing a stale id from a
    // previous session or an attacker — drop the whole in-flight.
    if (f.fragTotal != h.fragTotal) {
        inFlight_.erase(it);
        return std::nullopt;
    }

    if (f.present[h.fragIndex]) {
        // Duplicate fragment — ignore, not an error.
        return std::nullopt;
    }

    f.present[h.fragIndex] = true;
    f.pieces[h.fragIndex].assign(body, body + bodyBytes);
    ++f.received;

    if (f.received != f.fragTotal) return std::nullopt;

    // Complete! Concatenate pieces in order.
    std::size_t total = 0;
    for (const auto& p : f.pieces) total += p.size();
    FragmentPayload out;
    out.reserve(total);
    for (auto& p : f.pieces) {
        out.insert(out.end(), p.begin(), p.end());
    }
    inFlight_.erase(it);
    return out;
}

void Reassembler::gc(Clock::Millis nowMs, Clock::Millis timeoutMs) {
    for (auto it = inFlight_.begin(); it != inFlight_.end(); ) {
        const Clock::Millis age = nowMs - it->second.firstSeenMs;
        if (age > timeoutMs) {
            it = inFlight_.erase(it);
        } else {
            ++it;
        }
    }
}

void Reassembler::evictOldest() {
    if (inFlight_.empty()) return;
    auto oldest = inFlight_.begin();
    for (auto it = std::next(oldest); it != inFlight_.end(); ++it) {
        if (it->second.firstSeenMs < oldest->second.firstSeenMs) {
            oldest = it;
        }
    }
    inFlight_.erase(oldest);
}

}  // namespace engine::net
