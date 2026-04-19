/// @file test_quantize.cpp
/// @brief Unit tests for `engine::net::Quantize`.
///
/// Covers:
///   - Generic range packer: endpoints, midpoint, out-of-range clamp.
///   - Position helpers: bit width on the wire, error bound.
///   - Angle helpers: modulo-2π reduction, wrap-around, bit width.
///   - Stream helpers: round-trip through BitWriter/BitReader.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>

#include "engine/net/BitStream.h"
#include "engine/net/Quantize.h"

using engine::net::BitReader;
using engine::net::BitWriter;

namespace q = engine::net;

// Max quantisation error for a range over `numBits` bits.
static float stepOf(float min, float max, int numBits) {
    const uint32_t steps = (numBits == 32)
                               ? 0xFFFFFFFFu
                               : (1u << numBits) - 1u;
    return (max - min) / static_cast<float>(steps);
}

TEST_CASE("packFloatInRange: endpoints and midpoint") {
    // min, max, 8 bits -> 256 codes (0..255)
    CHECK(q::packFloatInRange(-1.0f, -1.0f, 1.0f, 8) == 0u);
    CHECK(q::packFloatInRange(+1.0f, -1.0f, 1.0f, 8) == 255u);
    CHECK(q::packFloatInRange( 0.0f, -1.0f, 1.0f, 8) == 128u);  // round up
}

TEST_CASE("packFloatInRange: clamps out-of-range inputs") {
    CHECK(q::packFloatInRange(-999.0f, -1.0f, 1.0f, 8) == 0u);
    CHECK(q::packFloatInRange(+999.0f, -1.0f, 1.0f, 8) == 255u);
}

TEST_CASE("packFloatInRange + unpackFloatInRange: error within one step") {
    const float min = -10.0f, max = 10.0f;
    const int bits = 12;
    const float step = stepOf(min, max, bits);

    for (float v : {-10.0f, -3.14f, 0.0f, 0.001f, 7.777f, 10.0f}) {
        const uint32_t code = q::packFloatInRange(v, min, max, bits);
        const float recon = q::unpackFloatInRange(code, min, max, bits);
        CHECK(std::fabs(recon - v) <= step);
    }
}

TEST_CASE("Position: round-trip within pixel-scale tolerance") {
    // One step of position = 32 / 65535 ≈ 4.88e-4
    const float step = stepOf(-q::kWorldExtent, +q::kWorldExtent, q::kPositionBits);

    for (float v : {-16.0f, -1.0f, -0.5f, 0.0f, 0.123f, 7.5f, 16.0f}) {
        const float recon = q::unpackPositionComponent(q::packPositionComponent(v));
        CHECK(std::fabs(recon - v) <= step);
    }
}

TEST_CASE("writePosition/readPosition: round-trip through BitStream") {
    BitWriter w;
    q::writePosition(w, -1.0f, 7.5f);
    q::writePosition(w, 0.0f, 0.0f);

    const auto& bytes = w.finish();
    // 2 positions × 2 axes × 16 bits = 64 bits = 8 bytes, no padding.
    CHECK(bytes.size() == 8u);

    BitReader r(bytes.data(), bytes.size());
    float x, y;
    const float step = stepOf(-q::kWorldExtent, +q::kWorldExtent, q::kPositionBits);

    q::readPosition(r, x, y);
    CHECK(std::fabs(x - (-1.0f)) <= step);
    CHECK(std::fabs(y - 7.5f)   <= step);

    q::readPosition(r, x, y);
    CHECK(std::fabs(x) <= step);
    CHECK(std::fabs(y) <= step);
    CHECK(r.ok());
}

TEST_CASE("Angle: reduces modulo 2pi before packing") {
    constexpr float kTwoPi = 6.28318530717958647692f;

    // These three should all pack to the same (or adjacent) code.
    const uint32_t a = q::packAngle(0.5f);
    const uint32_t b = q::packAngle(0.5f + kTwoPi);
    const uint32_t c = q::packAngle(0.5f - kTwoPi);

    // Exact equality modulo quantisation step — allow ±1 for rounding.
    auto close = [](uint32_t x, uint32_t y) {
        const int diff = static_cast<int>(x) - static_cast<int>(y);
        return diff >= -1 && diff <= 1;
    };
    CHECK(close(a, b));
    CHECK(close(a, c));
}

TEST_CASE("Angle: negative input reduced to [0, 2pi)") {
    // -pi/2 should round-trip to roughly 3*pi/2.
    constexpr float kHalfPi = 1.57079632679f;
    const uint32_t code = q::packAngle(-kHalfPi);
    const float recon = q::unpackAngle(code);

    // Step size for 8-bit angle over 2pi ≈ 0.0246 rad.
    const float step = stepOf(0.0f, 6.28318530717958647692f, q::kAngleBits);
    CHECK(std::fabs(recon - (3.0f * kHalfPi)) <= step);
}

TEST_CASE("writeAngle/readAngle: round-trip through BitStream") {
    BitWriter w;
    q::writeAngle(w, 1.0f);
    q::writeAngle(w, 4.7f);
    q::writeAngle(w, 0.0f);

    const auto& bytes = w.finish();
    // 3 angles × 8 bits = 24 bits = 3 bytes.
    CHECK(bytes.size() == 3u);

    BitReader r(bytes.data(), bytes.size());
    const float step = stepOf(0.0f, 6.28318530717958647692f, q::kAngleBits);

    float a;
    q::readAngle(r, a); CHECK(std::fabs(a - 1.0f) <= step);
    q::readAngle(r, a); CHECK(std::fabs(a - 4.7f) <= step);
    q::readAngle(r, a); CHECK(std::fabs(a - 0.0f) <= step);
    CHECK(r.ok());
}
