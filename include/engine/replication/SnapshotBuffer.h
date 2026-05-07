#pragma once

/// @file SnapshotBuffer.h
/// @brief Client-side ring buffer of server snapshots for smooth interpolation.
///        Phase 5a.
///
/// The server broadcasts world snapshots at a fixed rate (e.g. 20 Hz). The
/// client buffers them here and renders with a small fixed delay so there are
/// always two snapshots to interpolate between.
///
/// ## Render delay rationale
///
/// If the client renders at the server's "now" it must predict the future,
/// which is unreliable under jitter. By delaying render by ~2–3 snapshot
/// intervals (100–150 ms), the client always has the *past two* snapshots
/// and can interpolate with a simple linear blend. The perceptual cost is
/// minimal; the visual quality gain is significant.
///
/// ## What is stored
///
/// Only `Transform` components are kept per-entity — that is the data the
/// renderer needs for smooth motion. Velocity and other components are
/// applied immediately (not interpolated) when a snapshot arrives.
///
/// ## Thread safety
///
/// Not thread-safe. All calls must be from the same thread.
///
/// @see docs/design/0020-replication-model.md

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "engine/ecs/Entity.h"
#include "game/components/GameComponents.h"

namespace engine::replication {

/// Lightweight per-entity transform state captured at one server tick.
struct EntityState {
    engine::EntityId id        = 0;
    game::Transform  transform{};
};

/// All entity states for one server tick, decoded from a snapshot packet.
struct ReplicaSnapshot {
    std::uint32_t            tick = 0;
    std::vector<EntityState> entities;
};

/// Ring buffer of `ReplicaSnapshot` objects. Oldest entries are evicted when
/// the buffer is full; out-of-order (late) snapshots are discarded.
class SnapshotBuffer {
public:
    /// Maximum number of snapshots retained at once. At 20 Hz this covers
    /// 0.8 s of history — enough for any realistic RTT and jitter budget.
    static constexpr std::size_t kCapacity = 16;

    /// Push a snapshot received from the server.
    ///
    /// If the buffer is full, the oldest entry is evicted first. Snapshots
    /// with a `tick` ≤ the newest buffered tick are silently discarded
    /// (out-of-order / duplicate).
    void push(ReplicaSnapshot snap);

    /// True when `renderTick` falls within the range [oldestTick, newestTick]
    /// and at least two snapshots are buffered. The fractional part of
    /// `renderTick` represents the sub-tick alpha within the bracket.
    bool canInterpolate(float renderTick) const;

    /// Linearly interpolate the `Transform` for entity `id` at `renderTick`.
    ///
    /// `renderTick` is a fractional server-tick value (e.g. `7.3` means
    /// 30 % of the way from the snapshot at tick 7 to the one at tick 8).
    /// Returns `std::nullopt` when:
    ///   - fewer than two snapshots are buffered, or
    ///   - `renderTick` is outside the buffered range, or
    ///   - entity `id` is absent in either bounding snapshot.
    std::optional<game::Transform> interpolate(engine::EntityId id,
                                               float renderTick) const;

    std::size_t   size()       const { return buffer_.size(); }
    std::uint32_t newestTick() const;
    std::uint32_t oldestTick() const;

private:
    static const EntityState* findEntity(const ReplicaSnapshot& snap,
                                         engine::EntityId id);

    std::deque<ReplicaSnapshot> buffer_;  ///< oldest-first order
};

}  // namespace engine::replication
