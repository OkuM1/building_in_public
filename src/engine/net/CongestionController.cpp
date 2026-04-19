#include "engine/net/CongestionController.h"

namespace engine::net {

void CongestionController::update(double smoothedRttMs, Clock::Millis nowMs) {
    const bool rttBad = smoothedRttMs > cfg_.rttBadThresholdMs;

    if (mode_ == CongestionMode::Good) {
        if (rttBad) {
            // GOOD -> BAD. First drop uses initialPenaltyMs; subsequent
            // consecutive flaps double the penalty up to maxPenaltyMs.
            if (!haveEnteredBad_ || currentPenaltyMs_ == 0) {
                currentPenaltyMs_ = cfg_.initialPenaltyMs;
            } else {
                // Was previously BAD, recovered to GOOD, and now flapping
                // again — double the penalty.
                const Clock::Millis doubled = currentPenaltyMs_ * 2;
                currentPenaltyMs_ = doubled > cfg_.maxPenaltyMs
                    ? cfg_.maxPenaltyMs
                    : doubled;
            }
            mode_             = CongestionMode::Bad;
            enteredBadAtMs_   = nowMs;
            haveEnteredBad_   = true;
            haveRttGoodSample_ = false;
            return;
        }

        // Reset the penalty after a sustained period of good behaviour.
        if (haveEnteredBad_) {
            if (!haveRttGoodSample_) {
                rttGoodSinceMs_    = nowMs;
                haveRttGoodSample_ = true;
            }
            if ((nowMs - rttGoodSinceMs_) >= cfg_.penaltyResetAfterMs) {
                currentPenaltyMs_ = cfg_.initialPenaltyMs;
                haveEnteredBad_   = false;
            }
        }
        return;
    }

    // mode_ == CongestionMode::Bad
    if (rttBad) {
        // Still bad — reset the "good since" window so we don't
        // prematurely upgrade when it dips briefly.
        haveRttGoodSample_ = false;
        return;
    }

    // RTT dropped below threshold. Start / continue the stable-good
    // timer.
    if (!haveRttGoodSample_) {
        rttGoodSinceMs_    = nowMs;
        haveRttGoodSample_ = true;
    }

    // We can leave BAD iff:
    //   (a) the penalty timer since entering BAD has expired, AND
    //   (b) RTT has stayed under threshold for goodUpgradeAfterMs.
    const Clock::Millis sinceBad  = nowMs - enteredBadAtMs_;
    const Clock::Millis sinceGood = nowMs - rttGoodSinceMs_;
    if (sinceBad >= currentPenaltyMs_ && sinceGood >= cfg_.goodUpgradeAfterMs) {
        mode_ = CongestionMode::Good;
        // enteredBadAtMs_ is preserved; the NEXT GOOD->BAD flap reads
        // currentPenaltyMs_ and doubles it.
        rttGoodSinceMs_    = nowMs;
        haveRttGoodSample_ = true;
    }
}

}  // namespace engine::net
