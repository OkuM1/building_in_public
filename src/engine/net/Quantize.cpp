/// @file Quantize.cpp
/// @brief Implementation of fixed-point quantisation helpers.
///
/// @see include/engine/net/Quantize.h
/// @see docs/design/0009-quantization.md

#include "engine/net/Quantize.h"

#include <cmath>
#include <cstdint>

namespace engine::net {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

// Clamp without pulling in <algorithm>: keeps the TU light.
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Max integer representable in `numBits` bits (2^n - 1).
inline uint32_t maxCode(int numBits) {
    return numBits == 32 ? 0xFFFFFFFFu
                         : (uint32_t{1} << numBits) - 1u;
}

}  // namespace

uint32_t packFloatInRange(float value, float min, float max, int numBits) {
    const float clamped = clampf(value, min, max);
    const float range = max - min;
    const uint32_t steps = maxCode(numBits);
    const float normalised = (clamped - min) / range;
    // `+ 0.5` for round-to-nearest; `lroundf` is also fine but pulls
    // in more symbols. `static_cast` from a non-negative value is
    // defined behaviour.
    return static_cast<uint32_t>(normalised * static_cast<float>(steps) + 0.5f);
}

float unpackFloatInRange(uint32_t q, float min, float max, int numBits) {
    const uint32_t steps = maxCode(numBits);
    const float range = max - min;
    return min + (static_cast<float>(q) / static_cast<float>(steps)) * range;
}

// ---------------------------------------------------------------------------
// Position
// ---------------------------------------------------------------------------

uint32_t packPositionComponent(float v) {
    return packFloatInRange(v, -kWorldExtent, +kWorldExtent, kPositionBits);
}

float unpackPositionComponent(uint32_t q) {
    return unpackFloatInRange(q, -kWorldExtent, +kWorldExtent, kPositionBits);
}

void writePosition(BitWriter& w, float x, float y) {
    w.writeBits(packPositionComponent(x), kPositionBits);
    w.writeBits(packPositionComponent(y), kPositionBits);
}

void readPosition(BitReader& r, float& outX, float& outY) {
    const uint32_t qx = r.readBits(kPositionBits);
    const uint32_t qy = r.readBits(kPositionBits);
    outX = unpackPositionComponent(qx);
    outY = unpackPositionComponent(qy);
}

// ---------------------------------------------------------------------------
// Angle
// ---------------------------------------------------------------------------

uint32_t packAngle(float radians) {
    // Reduce to [0, 2*pi). `fmodf` can return negative; add once to fix.
    float reduced = std::fmod(radians, kTwoPi);
    if (reduced < 0.0f) reduced += kTwoPi;
    return packFloatInRange(reduced, 0.0f, kTwoPi, kAngleBits);
}

float unpackAngle(uint32_t q) {
    return unpackFloatInRange(q, 0.0f, kTwoPi, kAngleBits);
}

void writeAngle(BitWriter& w, float radians) {
    w.writeBits(packAngle(radians), kAngleBits);
}

void readAngle(BitReader& r, float& outRadians) {
    outRadians = unpackAngle(r.readBits(kAngleBits));
}

}  // namespace engine::net
