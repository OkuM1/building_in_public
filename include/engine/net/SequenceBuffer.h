#pragma once

// engine::net::SequenceBuffer<T, N> — sliding window keyed by
// sequence number. Header-only template.
//
// The stock netcode pattern (Gaffer "Building a Game Network Protocol"):
// we want to know "for sequence S, do I have an entry?" and
// "overwrite the entry for S" in O(1). A flat array of size N indexed
// by (sequence % N) plus a parallel `seqAt[i]` array is the standard
// trick: the seqAt slot tells us which sequence *currently* owns the
// slot (since many sequences hash to the same slot over time).
//
// N should be a power of two comfortably larger than the ack-bitfield
// width (32) so the buffer doesn't lose state before we've processed
// acks for it. 1024 is the usual choice.
//
// T is default-constructible; entries are value-initialised on insert.

#include <array>
#include <cstddef>
#include <cstdint>

namespace engine::net {

template <typename T, std::size_t N = 1024>
class SequenceBuffer {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
    // Reserve the entry for `seq` (overwriting whatever is there).
    // Returns a reference to the new entry, value-initialised.
    T& insert(std::uint16_t seq) {
        const std::size_t idx = seq & (N - 1);
        seqAt_[idx] = seq;
        present_[idx] = true;
        entries_[idx] = T{};
        return entries_[idx];
    }

    // Non-owning pointer to the entry for `seq`, or nullptr if the
    // slot is empty or currently owned by a different sequence.
    T* find(std::uint16_t seq) {
        const std::size_t idx = seq & (N - 1);
        if (!present_[idx] || seqAt_[idx] != seq) return nullptr;
        return &entries_[idx];
    }

    bool exists(std::uint16_t seq) const {
        const std::size_t idx = seq & (N - 1);
        return present_[idx] && seqAt_[idx] == seq;
    }

    void remove(std::uint16_t seq) {
        const std::size_t idx = seq & (N - 1);
        if (present_[idx] && seqAt_[idx] == seq) {
            present_[idx] = false;
        }
    }

    static constexpr std::size_t size() { return N; }

private:
    std::array<T, N>             entries_{};
    std::array<std::uint16_t, N> seqAt_{};     // which seq currently owns each slot
    std::array<bool, N>          present_{};   // whether this slot is live
};

}  // namespace engine::net
