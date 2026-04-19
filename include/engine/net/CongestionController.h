#pragma once

// engine::net::CongestionController — Gaffer "good mode / bad mode"
// send-rate controller. Phase 4e.
//
// Problem: if we blindly send at 60 Hz regardless of whether the link
// is coping, a congested link gets worse. A real congestion-control
// algorithm (BBR, Cubic) is overkill for a game engine; Gaffer's
// 2-state heuristic is well-matched to our traffic profile (small,
// regular packets), easy to reason about, and easy to test.
//
// Two modes, two send rates:
//
//   GOOD:  send at goodRateHz (e.g. 30 Hz).
//   BAD:   send at badRateHz  (e.g. 10 Hz).
//
// Transitions:
//
//   GOOD -> BAD   when smoothed RTT crosses rttBadThresholdMs.
//                 Immediately after a drop, a "penalty" timer keeps
//                 us in BAD for at least minBadDurationMs so the
//                 controller can't oscillate.
//
//   BAD  -> GOOD  when RTT has stayed under rttBadThresholdMs for
//                 goodUpgradeAfterMs AND the penalty timer has
//                 expired. The penalty doubles on every consecutive
//                 GOOD->BAD->GOOD flap so a flappy link burns itself
//                 into BAD mode (Gaffer's "penalty time" mechanic).
//
// The controller owns NO socket, NO endpoint, NO clock other than
// via caller-supplied Millis — same pattern as ReliableEndpoint
// (see [0014]).
//
// What the controller produces
// ----------------------------
// `sendRateHz()` is the only public output. Callers turn that into
// a "time until next send" via `millisBetweenSends()` and use it to
// gate their per-tick net pump.
//
// The controller does NOT throttle individual packet sizes — that's
// a per-packet budget decided by the MTU machinery in Phase 4d.
// Rate control and size control are independent levers.
//
// See docs/design/0017-congestion-control.md.

#include <cstdint>

#include "engine/net/ReliableEndpoint.h"  // for Clock::Millis

namespace engine::net {

enum class CongestionMode : std::uint8_t {
    Good,
    Bad,
};

// All knobs live in one struct so tuning is auditable and tests can
// dial parameters without touching the class internals.
struct CongestionConfig {
    // Send rates (Hz) for each mode.
    double goodRateHz = 30.0;
    double badRateHz  = 10.0;

    // RTT threshold (ms). Crossing this flips GOOD->BAD.
    double rttBadThresholdMs = 250.0;

    // Minimum time we must stay in BAD after any transition.
    // Doubles on each consecutive flap, capped at maxPenaltyMs.
    Clock::Millis initialPenaltyMs = 1000;
    Clock::Millis maxPenaltyMs     = 60000;

    // How long the RTT must stay below threshold before we upgrade
    // BAD -> GOOD.
    Clock::Millis goodUpgradeAfterMs = 10000;

    // A link that behaves well for this long in GOOD resets the
    // penalty to initialPenaltyMs.
    Clock::Millis penaltyResetAfterMs = 10000;
};

class CongestionController {
public:
    explicit CongestionController(CongestionConfig cfg = {}) : cfg_(cfg) {}

    // Feed the controller the latest smoothed RTT from
    // ReliableEndpoint, plus current time. Must be called regularly
    // (per received ack, or at least per tick) — the controller's
    // state machine only advances on this call.
    void update(double smoothedRttMs, Clock::Millis nowMs);

    // Output: current send rate in Hz.
    double sendRateHz() const {
        return mode_ == CongestionMode::Good ? cfg_.goodRateHz : cfg_.badRateHz;
    }

    // Convenience: milliseconds to wait between sends. Always > 0.
    double millisBetweenSends() const {
        const double r = sendRateHz();
        return r > 0.0 ? 1000.0 / r : 1.0e9;
    }

    CongestionMode mode() const { return mode_; }

    // Visible for tests / metrics.
    Clock::Millis currentPenaltyMs() const { return currentPenaltyMs_; }

private:
    CongestionConfig cfg_;
    CongestionMode   mode_ = CongestionMode::Good;

    // When the last GOOD->BAD transition happened. Zero means never.
    Clock::Millis enteredBadAtMs_ = 0;
    bool          haveEnteredBad_ = false;

    // How long we must stay in BAD after the latest transition.
    // Doubles on every flap up to cfg_.maxPenaltyMs.
    Clock::Millis currentPenaltyMs_ = 0;

    // Timestamp the RTT most recently dropped below threshold
    // (or when we last entered GOOD after a flap).
    Clock::Millis rttGoodSinceMs_ = 0;
    bool          haveRttGoodSample_ = false;
};

}  // namespace engine::net
