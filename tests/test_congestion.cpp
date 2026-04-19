// Phase 4e: CongestionController good/bad-mode state machine.

#include <doctest.h>

#include "engine/net/CongestionController.h"

using namespace engine::net;

namespace {

// Tight config to keep tests quick and readable.
CongestionConfig fastConfig() {
    CongestionConfig c;
    c.goodRateHz            = 30.0;
    c.badRateHz             = 10.0;
    c.rttBadThresholdMs     = 250.0;
    c.initialPenaltyMs      = 1000;
    c.maxPenaltyMs          = 16000;
    c.goodUpgradeAfterMs    = 2000;
    c.penaltyResetAfterMs   = 3000;
    return c;
}

}  // namespace

TEST_CASE("CongestionController starts in GOOD mode at the good rate") {
    CongestionController cc;
    CHECK(cc.mode() == CongestionMode::Good);
    CHECK(cc.sendRateHz() == doctest::Approx(30.0));
}

TEST_CASE("CongestionController: high RTT flips GOOD -> BAD immediately") {
    CongestionController cc{fastConfig()};
    cc.update(/*rtt=*/300.0, /*now=*/0);
    CHECK(cc.mode() == CongestionMode::Bad);
    CHECK(cc.sendRateHz() == doctest::Approx(10.0));
    CHECK(cc.currentPenaltyMs() == 1000);
}

TEST_CASE("CongestionController: stays in BAD until penalty expires AND RTT is stable-good") {
    CongestionController cc{fastConfig()};

    cc.update(300.0, 0);         // -> BAD, penalty 1000
    cc.update(100.0, 500);       // RTT fine but only 500 ms in BAD
    CHECK(cc.mode() == CongestionMode::Bad);

    // Penalty expired (1000 ms) but stable-good window (2000 ms) hasn't.
    cc.update(100.0, 1100);
    CHECK(cc.mode() == CongestionMode::Bad);

    // Still RTT-good; now 2000 ms since rtt first became good (at t=500).
    cc.update(100.0, 2500);
    CHECK(cc.mode() == CongestionMode::Good);
}

TEST_CASE("CongestionController: a brief RTT dip in BAD doesn't reset the good-since timer prematurely") {
    CongestionController cc{fastConfig()};

    cc.update(300.0, 0);         // -> BAD
    cc.update(100.0, 500);       // rttGoodSince = 500
    cc.update(400.0, 700);       // RTT spikes again -> good-since invalidated
    // Penalty has expired, but stable-good window must restart.
    cc.update(100.0, 1000);      // rttGoodSince = 1000
    cc.update(100.0, 2900);      // only 1900 ms stable -> still BAD
    CHECK(cc.mode() == CongestionMode::Bad);

    cc.update(100.0, 3100);      // 2100 ms stable -> upgrade
    CHECK(cc.mode() == CongestionMode::Good);
}

TEST_CASE("CongestionController: consecutive flap doubles the penalty") {
    CongestionController cc{fastConfig()};

    // Flap 1: GOOD -> BAD -> GOOD. Penalty = 1000.
    cc.update(300.0, 0);
    cc.update(100.0, 1000);
    cc.update(100.0, 3500);      // penalty 1000 elapsed + 2500 stable-good -> GOOD
    REQUIRE(cc.mode() == CongestionMode::Good);
    CHECK(cc.currentPenaltyMs() == 1000);

    // Flap 2: goes BAD again BEFORE penaltyResetAfterMs (3000 ms since
    // recovery, so we're still inside the reset window at t=3500+.
    // Trigger the flap promptly at t=4000.
    cc.update(300.0, 4000);
    CHECK(cc.mode() == CongestionMode::Bad);
    CHECK(cc.currentPenaltyMs() == 2000);  // doubled
}

TEST_CASE("CongestionController: sustained GOOD behaviour resets the penalty") {
    CongestionConfig cfg = fastConfig();
    CongestionController cc{cfg};

    cc.update(300.0, 0);
    cc.update(100.0, 1000);
    cc.update(100.0, 3500);      // -> GOOD with penalty 1000 still cached
    REQUIRE(cc.mode() == CongestionMode::Good);

    // Stay in GOOD for longer than penaltyResetAfterMs (3000 ms).
    cc.update(100.0, 7000);
    // Now a new flap should be charged the fresh initial penalty, not a doubled one.
    cc.update(300.0, 8000);
    CHECK(cc.mode() == CongestionMode::Bad);
    CHECK(cc.currentPenaltyMs() == 1000);
}

TEST_CASE("CongestionController: penalty is capped at maxPenaltyMs") {
    CongestionConfig cfg;
    cfg.initialPenaltyMs    = 1000;
    cfg.maxPenaltyMs        = 4000;
    cfg.goodUpgradeAfterMs  = 500;
    cfg.penaltyResetAfterMs = 100000;  // effectively never reset
    CongestionController cc{cfg};

    Clock::Millis t = 0;
    // Flap five times; penalty should double until it saturates at 4000.
    const std::uint64_t expected[] = {1000, 2000, 4000, 4000, 4000};
    for (int i = 0; i < 5; ++i) {
        cc.update(300.0, t);                 // -> BAD
        CHECK(cc.mode() == CongestionMode::Bad);
        CHECK(cc.currentPenaltyMs() == expected[i]);

        // Start the stable-good timer now...
        t += 10;
        cc.update(100.0, t);
        // ...then wait long enough that BOTH the penalty (since BAD
        // entry) and the stable-good window (since this sample) have
        // elapsed.
        t += cc.currentPenaltyMs() + cfg.goodUpgradeAfterMs + 10;
        cc.update(100.0, t);
        REQUIRE(cc.mode() == CongestionMode::Good);
        t += 10;
    }
}

TEST_CASE("CongestionController: send rates derive from mode") {
    CongestionController cc{fastConfig()};
    CHECK(cc.sendRateHz() == doctest::Approx(30.0));
    CHECK(cc.millisBetweenSends() == doctest::Approx(1000.0 / 30.0));

    cc.update(300.0, 0);
    CHECK(cc.sendRateHz() == doctest::Approx(10.0));
    CHECK(cc.millisBetweenSends() == doctest::Approx(100.0));
}
