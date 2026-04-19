#pragma once

// engine::net::SimulatedLink — in-process packet simulator. Phase 4f.
//
// Purpose: exercise the full reliable-UDP stack (ack + channels +
// fragmentation + congestion) on one machine under reproducible,
// adversarial conditions — 150 ms RTT, 5 % loss, jitter, reordering —
// without touching a real socket.
//
// Shape:
//
//   +---------+   send()     +------------------+   receive()   +---------+
//   | sender  | -----------> |  SimulatedLink   | ------------> |receiver |
//   +---------+              +------------------+               +---------+
//
// Every `send(bytes, nowMs)` enqueues the packet with a per-packet
// delivery time = `nowMs + oneWayLatency + jitter`. Calls to
// `receive(nowMs)` return packets whose delivery time has passed,
// in delivery-time order — this is where reordering falls out for
// free: jitter can put a later-sent packet ahead of an earlier one.
//
// Loss is a Bernoulli trial on `send()`. A lost packet is silently
// dropped — no placeholder, no delivery record. This matches how
// real UDP loss looks to the receiver.
//
// Determinism: a caller-supplied 64-bit seed feeds a `std::mt19937_64`.
// Same seed + same call sequence = same packets delivered in the
// same order. That is the whole point — tests that fail once must
// fail every time.
//
// What this class does NOT do:
//   * No bandwidth cap. The congestion controller (Phase 4e) lives
//     on the sender side; this simulator models pipe behaviour, not
//     pipe capacity.
//   * No duplication (yet). Real-world UDP duplicates are rare and
//     channels already dedupe. Add if Phase 5 needs it.
//   * No MTU enforcement. Fragmentation is a sender-side concern.
//
// See docs/design/0018-network-simulator.md.

#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <vector>

#include "engine/net/ReliableEndpoint.h"  // Clock::Millis

namespace engine::net {

struct LinkConfig {
    // One-way latency floor applied to every delivered packet (ms).
    double oneWayLatencyMs = 75.0;

    // Uniform jitter on top of the floor, in ms. Added as +/- range,
    // so effective one-way delay is in
    //   [oneWayLatencyMs - jitterMs, oneWayLatencyMs + jitterMs].
    // Jitter is what produces reordering.
    double jitterMs = 0.0;

    // Independent per-packet drop probability in [0, 1]. 0.05 = 5 %.
    double lossProbability = 0.0;

    // RNG seed. Same seed -> same outcomes.
    std::uint64_t seed = 0xC0FFEEULL;
};

class SimulatedLink {
public:
    explicit SimulatedLink(LinkConfig cfg = {})
        : cfg_(cfg), rng_(cfg.seed) {}

    // Offer a packet to the link at current time `nowMs`.
    // Returns true if the packet was queued for delivery, false if
    // it was dropped by the loss model.
    bool send(const std::uint8_t* data, std::size_t size, Clock::Millis nowMs);

    // Drain all packets whose delivery time is <= nowMs, returned in
    // delivery-time order.
    std::vector<std::vector<std::uint8_t>> receive(Clock::Millis nowMs);

    // Counters (useful for tests and metrics).
    std::uint64_t sentCount()      const { return sentCount_; }
    std::uint64_t droppedCount()   const { return droppedCount_; }
    std::uint64_t deliveredCount() const { return deliveredCount_; }
    std::size_t   inFlight()       const { return inFlight_.size(); }

    const LinkConfig& config() const { return cfg_; }

private:
    struct InFlight {
        Clock::Millis               deliverAtMs;
        std::uint64_t               sequence;  // monotonic, for stable ordering
        std::vector<std::uint8_t>   data;

        // Priority queue orders smallest deliverAtMs first; ties
        // broken by insertion sequence so equal-delay packets are
        // delivered FIFO.
        bool operator>(const InFlight& o) const {
            if (deliverAtMs != o.deliverAtMs) return deliverAtMs > o.deliverAtMs;
            return sequence > o.sequence;
        }
    };

    LinkConfig          cfg_;
    std::mt19937_64     rng_;
    std::priority_queue<InFlight, std::vector<InFlight>, std::greater<InFlight>> inFlight_;
    std::uint64_t       nextSeq_       = 0;
    std::uint64_t       sentCount_     = 0;
    std::uint64_t       droppedCount_  = 0;
    std::uint64_t       deliveredCount_ = 0;
};

}  // namespace engine::net
