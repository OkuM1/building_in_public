#include "engine/replication/PredictionBuffer.h"

namespace engine::replication {

void PredictionBuffer::push(std::uint32_t tick, const game::PlayerInput& input,
                             const game::Transform& predicted) {
    if (buffer_.size() >= kCapacity) buffer_.pop_front();
    buffer_.push_back({tick, input, predicted});
}

void PredictionBuffer::ackUpTo(std::uint32_t ackedTick) {
    while (!buffer_.empty() && buffer_.front().tick <= ackedTick) {
        buffer_.pop_front();
    }
}

std::vector<PredictionEntry> PredictionBuffer::getPending(
    std::uint32_t afterTick) const {
    std::vector<PredictionEntry> result;
    for (const auto& e : buffer_) {
        if (e.tick > afterTick) result.push_back(e);
    }
    return result;
}

const PredictionEntry* PredictionBuffer::get(std::uint32_t tick) const {
    for (const auto& e : buffer_) {
        if (e.tick == tick) return &e;
    }
    return nullptr;
}

std::uint32_t PredictionBuffer::oldestTick() const {
    return buffer_.empty() ? 0 : buffer_.front().tick;
}

std::uint32_t PredictionBuffer::newestTick() const {
    return buffer_.empty() ? 0 : buffer_.back().tick;
}

}  // namespace engine::replication
