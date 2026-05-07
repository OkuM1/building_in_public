#pragma once

/// @file PredictionBuffer.h
/// @brief Client-side ring buffer for client-side prediction + reconciliation.
///        Phase 5a.
///
/// ## Client-side prediction
///
/// The client applies its own input immediately (before the server confirms
/// it) and records the resulting transform in this buffer. This eliminates
/// input latency for the local player: movement feels instant.
///
/// ## Reconciliation
///
/// When the server's authoritative snapshot for tick T arrives, the client:
///
///   1. Calls `ackUpTo(T)` to discard confirmed entries (they are history).
///   2. Compares the server's authoritative transform at T with
///      `get(T)->predictedTransform` (if available).
///   3. If the difference exceeds a threshold ("misprediction"), the client
///      snaps to the authoritative state and calls `getPending(T)` to replay
///      all unconfirmed inputs forward from T.
///   4. After replay the new predicted transforms replace the old ones.
///
/// This "snap and replay" approach is the standard technique described by
/// Gabriel Gambetta and used by the Quake/Source engine family.
///
/// ## Capacity
///
/// At 60 Hz, 128 ticks ≈ 2.1 s. Any real-world RTT fits comfortably within
/// this window. If the buffer overflows (RTT > 2 s) the oldest entry is
/// silently evicted — the client will reconcile against whatever the server
/// last confirmed.
///
/// @see docs/design/0020-replication-model.md

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "game/components/GameComponents.h"

namespace engine::replication {

/// One tick's worth of prediction state retained for possible reconciliation.
struct PredictionEntry {
    std::uint32_t     tick;
    game::PlayerInput input;
    game::Transform   predictedTransform;
};

/// Ring buffer of prediction entries ordered by ascending tick.
class PredictionBuffer {
public:
    /// Maximum unacknowledged ticks stored at once. At 60 Hz this covers
    /// ~2.1 s — enough for any realistic RTT.
    static constexpr std::size_t kCapacity = 128;

    /// Record a tick's input and locally-predicted result.
    /// Drops the oldest entry when the buffer is full.
    void push(std::uint32_t tick, const game::PlayerInput& input,
              const game::Transform& predicted);

    /// Discard all entries with tick ≤ `ackedTick`.
    /// Called when the server confirms it processed inputs up to that tick.
    void ackUpTo(std::uint32_t ackedTick);

    /// Return all entries with tick > `afterTick`, in ascending tick order.
    /// Used to replay unconfirmed inputs when reconciling against a server
    /// snapshot.
    std::vector<PredictionEntry> getPending(std::uint32_t afterTick) const;

    /// Look up an entry by tick. Returns nullptr if not present.
    const PredictionEntry* get(std::uint32_t tick) const;

    std::size_t   size()       const { return buffer_.size(); }
    bool          empty()      const { return buffer_.empty(); }
    std::uint32_t oldestTick() const;
    std::uint32_t newestTick() const;

private:
    std::deque<PredictionEntry> buffer_;  ///< ascending tick order
};

}  // namespace engine::replication
