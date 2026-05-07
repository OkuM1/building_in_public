#include "engine/replication/SnapshotBuffer.h"

namespace engine::replication {

void SnapshotBuffer::push(ReplicaSnapshot snap) {
    // Discard out-of-order or duplicate snapshots.
    if (!buffer_.empty() && snap.tick <= buffer_.back().tick) return;

    if (buffer_.size() >= kCapacity) buffer_.pop_front();
    buffer_.push_back(std::move(snap));
}

bool SnapshotBuffer::canInterpolate(float renderTick) const {
    if (buffer_.size() < 2) return false;
    const float tOld = static_cast<float>(buffer_.front().tick);
    const float tNew = static_cast<float>(buffer_.back().tick);
    return renderTick >= tOld && renderTick <= tNew;
}

std::optional<game::Transform> SnapshotBuffer::interpolate(
    engine::EntityId id, float renderTick) const {
    if (buffer_.size() < 2) return std::nullopt;

    // Find two adjacent snapshots that bracket renderTick.
    const ReplicaSnapshot* from = nullptr;
    const ReplicaSnapshot* to   = nullptr;

    for (std::size_t i = 0; i + 1 < buffer_.size(); ++i) {
        const float t0 = static_cast<float>(buffer_[i].tick);
        const float t1 = static_cast<float>(buffer_[i + 1].tick);
        if (renderTick >= t0 && renderTick < t1) {
            from = &buffer_[i];
            to   = &buffer_[i + 1];
            break;
        }
    }

    // Allow exact match at the newest snapshot tick.
    if (!from && renderTick == static_cast<float>(buffer_.back().tick)
        && buffer_.size() >= 2) {
        from = &buffer_[buffer_.size() - 2];
        to   = &buffer_.back();
    }

    if (!from) return std::nullopt;

    const EntityState* a = findEntity(*from, id);
    const EntityState* b = findEntity(*to,   id);
    if (!a || !b) return std::nullopt;

    const float dt = static_cast<float>(to->tick - from->tick);
    const float alpha = (dt > 0.0f)
        ? (renderTick - static_cast<float>(from->tick)) / dt
        : 0.0f;

    game::Transform result;
    result.x        = a->transform.x        + alpha * (b->transform.x        - a->transform.x);
    result.y        = a->transform.y        + alpha * (b->transform.y        - a->transform.y);
    result.rotation = a->transform.rotation + alpha * (b->transform.rotation - a->transform.rotation);
    return result;
}

std::uint32_t SnapshotBuffer::newestTick() const {
    return buffer_.empty() ? 0 : buffer_.back().tick;
}

std::uint32_t SnapshotBuffer::oldestTick() const {
    return buffer_.empty() ? 0 : buffer_.front().tick;
}

const EntityState* SnapshotBuffer::findEntity(const ReplicaSnapshot& snap,
                                               engine::EntityId id) {
    for (const auto& e : snap.entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

}  // namespace engine::replication
