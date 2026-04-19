#include "engine/net/SimulatedLink.h"

#include <algorithm>

namespace engine::net {

bool SimulatedLink::send(const std::uint8_t* data, std::size_t size, Clock::Millis nowMs) {
    ++sentCount_;

    // Bernoulli drop. Draw regardless of probability so the RNG
    // stream is independent of loss rate — tests that dial loss up
    // and down still get reproducible delays for the surviving
    // packets.
    std::uniform_real_distribution<double> dropDist(0.0, 1.0);
    const double dropRoll = dropDist(rng_);

    // Jitter draw, always taken for stream stability.
    std::uniform_real_distribution<double> jitterDist(-cfg_.jitterMs, cfg_.jitterMs);
    const double jitter = cfg_.jitterMs > 0.0 ? jitterDist(rng_) : 0.0;

    if (dropRoll < cfg_.lossProbability) {
        ++droppedCount_;
        return false;
    }

    double delayMs = cfg_.oneWayLatencyMs + jitter;
    if (delayMs < 0.0) delayMs = 0.0;

    InFlight packet;
    packet.deliverAtMs = nowMs + static_cast<Clock::Millis>(delayMs);
    packet.sequence    = nextSeq_++;
    packet.data.assign(data, data + size);

    inFlight_.push(std::move(packet));
    return true;
}

std::vector<std::vector<std::uint8_t>> SimulatedLink::receive(Clock::Millis nowMs) {
    std::vector<std::vector<std::uint8_t>> out;
    while (!inFlight_.empty() && inFlight_.top().deliverAtMs <= nowMs) {
        // priority_queue::top() returns const&; we move out via a copy
        // of data after popping.
        InFlight next = inFlight_.top();
        inFlight_.pop();
        out.push_back(std::move(next.data));
        ++deliveredCount_;
    }
    return out;
}

}  // namespace engine::net
