// Phase 4d: fragmentation + reassembly.

#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "engine/net/Fragmentation.h"

using namespace engine::net;

namespace {

// Deterministic byte pattern so assertions are easy to read.
FragmentPayload makePayload(std::size_t n, std::uint8_t seed = 0) {
    FragmentPayload p(n);
    for (std::size_t i = 0; i < n; ++i) {
        p[i] = static_cast<std::uint8_t>((seed + i * 31u) & 0xFF);
    }
    return p;
}

}  // namespace

TEST_CASE("FragmentHeader round-trips through wire bytes") {
    std::array<std::uint8_t, kFragmentHeaderBytes> buf{};
    FragmentHeader h{1234, 7, 16};
    REQUIRE(writeFragmentHeader(h, buf.data(), buf.size()));
    FragmentHeader got;
    REQUIRE(readFragmentHeader(buf.data(), buf.size(), got));
    CHECK(got.messageId == 1234);
    CHECK(got.fragIndex == 7);
    CHECK(got.fragTotal == 16);
}

TEST_CASE("fragment: small payload produces a single fragment") {
    const auto p = makePayload(10);
    const auto frags = fragment(1, p.data(), p.size(), /*maxFragmentPayload=*/1200);
    REQUIRE(frags.size() == 1);
    CHECK(frags[0].size() == kFragmentHeaderBytes + p.size());

    FragmentHeader h;
    REQUIRE(readFragmentHeader(frags[0].data(), frags[0].size(), h));
    CHECK(h.messageId == 1);
    CHECK(h.fragIndex == 0);
    CHECK(h.fragTotal == 1);
}

TEST_CASE("fragment: oversized payload splits evenly") {
    const std::size_t maxBody = 100;
    const auto p = makePayload(350);  // ceil(350/100) = 4 fragments
    const auto frags = fragment(42, p.data(), p.size(), maxBody);
    REQUIRE(frags.size() == 4);

    // Fragments 0..2 are full-sized; last one is 50 bytes of payload.
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(frags[i].size() == kFragmentHeaderBytes + maxBody);
    }
    CHECK(frags[3].size() == kFragmentHeaderBytes + 50);

    // Header fields are consistent across the set.
    for (std::uint16_t i = 0; i < 4; ++i) {
        FragmentHeader h;
        REQUIRE(readFragmentHeader(frags[i].data(), frags[i].size(), h));
        CHECK(h.messageId == 42);
        CHECK(h.fragIndex == i);
        CHECK(h.fragTotal == 4);
    }
}

TEST_CASE("fragment: returns empty when fragCount exceeds safety cap") {
    const auto p = makePayload(1000);
    // max body = 1 byte would need 1000 fragments, cap is 256.
    const auto frags = fragment(1, p.data(), p.size(), 1);
    CHECK(frags.empty());
}

TEST_CASE("Reassembler: in-order fragments reassemble to original payload") {
    const auto p = makePayload(500);
    const auto frags = fragment(7, p.data(), p.size(), /*maxBody=*/120);
    REQUIRE(frags.size() == 5);

    Reassembler r;
    std::optional<FragmentPayload> out;
    for (std::size_t i = 0; i < frags.size(); ++i) {
        out = r.offer(frags[i].data(), frags[i].size(), /*now=*/i);
        if (i + 1 < frags.size()) CHECK_FALSE(out.has_value());
    }
    REQUIRE(out.has_value());
    CHECK(*out == p);
    CHECK(r.inFlightCount() == 0);
}

TEST_CASE("Reassembler: shuffled fragments still reassemble correctly") {
    const auto p = makePayload(777, /*seed=*/11);
    auto frags = fragment(9, p.data(), p.size(), /*maxBody=*/50);
    REQUIRE(frags.size() == 16);

    std::mt19937 rng{1234};
    std::shuffle(frags.begin(), frags.end(), rng);

    Reassembler r;
    std::optional<FragmentPayload> out;
    for (std::size_t i = 0; i < frags.size(); ++i) {
        out = r.offer(frags[i].data(), frags[i].size(), /*now=*/i);
    }
    REQUIRE(out.has_value());
    CHECK(*out == p);
}

TEST_CASE("Reassembler: duplicate fragments are safely ignored") {
    const auto p = makePayload(200);
    const auto frags = fragment(3, p.data(), p.size(), /*maxBody=*/80);
    REQUIRE(frags.size() == 3);

    Reassembler r;
    // First fragment delivered twice, then the rest.
    CHECK_FALSE(r.offer(frags[0].data(), frags[0].size(), 0).has_value());
    CHECK_FALSE(r.offer(frags[0].data(), frags[0].size(), 1).has_value());
    CHECK_FALSE(r.offer(frags[1].data(), frags[1].size(), 2).has_value());
    auto out = r.offer(frags[2].data(), frags[2].size(), 3);
    REQUIRE(out.has_value());
    CHECK(*out == p);
}

TEST_CASE("Reassembler: malformed input returns nullopt") {
    Reassembler r;
    // Too short to contain a header.
    std::array<std::uint8_t, 2> tooShort{};
    CHECK_FALSE(r.offer(tooShort.data(), tooShort.size(), 0).has_value());

    // fragIndex >= fragTotal
    std::array<std::uint8_t, kFragmentHeaderBytes> bad{};
    FragmentHeader h{1, /*idx=*/5, /*total=*/3};
    writeFragmentHeader(h, bad.data(), bad.size());
    CHECK_FALSE(r.offer(bad.data(), bad.size(), 0).has_value());

    // fragTotal == 0
    FragmentHeader h2{1, 0, 0};
    writeFragmentHeader(h2, bad.data(), bad.size());
    CHECK_FALSE(r.offer(bad.data(), bad.size(), 0).has_value());
}

TEST_CASE("Reassembler: gc expires incomplete sets past the timeout") {
    const auto p = makePayload(300);
    const auto frags = fragment(55, p.data(), p.size(), /*maxBody=*/100);
    REQUIRE(frags.size() == 3);

    Reassembler r;
    r.offer(frags[0].data(), frags[0].size(), /*now=*/1000);
    r.offer(frags[1].data(), frags[1].size(), /*now=*/1100);
    CHECK(r.inFlightCount() == 1);

    // GC at t=1500 with timeout=2000 → still young, keep.
    r.gc(/*now=*/1500, /*timeoutMs=*/2000);
    CHECK(r.inFlightCount() == 1);

    // GC at t=4000 with timeout=2000 → 3000ms old, expire.
    r.gc(/*now=*/4000, /*timeoutMs=*/2000);
    CHECK(r.inFlightCount() == 0);

    // Fragment 2 arriving after expiry starts a fresh in-flight entry
    // (it looks exactly like a fresh message to the reassembler — the
    // fragTotal matches and there's nothing to contradict it), and the
    // message will eventually time out as incomplete. That's the
    // correct behaviour: we would rather drop the payload than deliver
    // a half-stale one.
    CHECK_FALSE(r.offer(frags[2].data(), frags[2].size(), /*now=*/4100).has_value());
    CHECK(r.inFlightCount() == 1);
}

TEST_CASE("Reassembler: mismatched fragTotal for same messageId drops the set") {
    Reassembler r;

    // Peer A claims fragTotal=3.
    std::array<std::uint8_t, kFragmentHeaderBytes + 1> f0{};
    FragmentHeader h0{77, 0, 3};
    writeFragmentHeader(h0, f0.data(), f0.size());
    f0[kFragmentHeaderBytes] = 0xAA;
    CHECK_FALSE(r.offer(f0.data(), f0.size(), 0).has_value());
    CHECK(r.inFlightCount() == 1);

    // Then something with the same messageId claims fragTotal=5.
    std::array<std::uint8_t, kFragmentHeaderBytes + 1> f1{};
    FragmentHeader h1{77, 1, 5};
    writeFragmentHeader(h1, f1.data(), f1.size());
    f1[kFragmentHeaderBytes] = 0xBB;
    CHECK_FALSE(r.offer(f1.data(), f1.size(), 0).has_value());

    // The whole in-flight for message 77 should have been discarded.
    CHECK(r.inFlightCount() == 0);
}
