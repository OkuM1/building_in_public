/// @file Quantize.h
/// @brief Fixed-point quantisation helpers for wire serialization.
///
/// Floats are expensive on the wire: a `float` spends 32 bits on a
/// range and precision we almost never need. A position inside a
/// bounded arena doesn't need `1e-38` to `1e38` with 24 bits of
/// mantissa — it needs `[-16, +16]` with ~0.5 mm resolution, which
/// fits in 16 bits.
///
/// These are **pure, platform-free** helpers. They live in the
/// `simulation` target so the server and tests can round-trip snapshots
/// without linking any windowing library.
///
/// The quantiser is a simple uniform-range encoder:
///
///     q    = round((v - min) / (max - min) * (2^bits - 1))    [clamped]
///     v'   = min + q * (max - min) / (2^bits - 1)
///
/// Error bound: half a step. For position (16 bits, range 32) this is
/// `32 / 2 / 65535 ≈ 2.44e-4` — well under a pixel at any sane scale.
///
/// @note Uniform quantisation is isotropic (same error everywhere in
///       the range). If we later need finer precision near zero we'd
///       swap in a variable-precision encoder; until then uniform is
///       correct and simpler.
///
/// @see docs/design/0009-quantization.md
#pragma once

#include <cstdint>

#include "engine/net/BitStream.h"

namespace engine::net {

// ---------------------------------------------------------------------------
// Tunables. Centralised here so a single recompile shifts the wire
// format across the whole codebase.
// ---------------------------------------------------------------------------

/// Half-extent of the arena on each axis. World positions are expected
/// in the closed range `[-kWorldExtent, +kWorldExtent]`. Values outside
/// this range are clamped on encode (a lossy but recoverable failure).
///
/// Current game loop runs in NDC (`[-1, 1]`), but we size the wire
/// format for 16 units so Sumo Arena and any future arena scale up
/// without breaking snapshot compatibility.
constexpr float kWorldExtent = 16.0f;

/// Bits used per position component on the wire. 16 bits over the
/// `32`-unit range yields ~2.4e-4 max quantisation error per axis.
constexpr int kPositionBits = 16;

/// Bits used for a rotation in `[0, 2*pi)`. 8 bits = ~1.4° precision,
/// which is below human perceptual threshold for the Sumo Arena camera.
constexpr int kAngleBits = 8;

// ---------------------------------------------------------------------------
// Generic uniform quantiser. `numBits` must be in [1, 32].
// ---------------------------------------------------------------------------

/// Pack `value` into `numBits` bits uniformly over `[min, max]`.
/// Out-of-range inputs are clamped to the endpoints.
uint32_t packFloatInRange(float value, float min, float max, int numBits);

/// Inverse of `packFloatInRange`. Returns a representative float in
/// `[min, max]`. The round-trip error is bounded by one step size.
float unpackFloatInRange(uint32_t q, float min, float max, int numBits);

// ---------------------------------------------------------------------------
// Typed helpers that also write/read through a BitStream. The stream
// path is the one the snapshot encoder (Commit 3) will use; the plain
// `packPositionComponent` overload exists for tests and benchmarks.
// ---------------------------------------------------------------------------

/// Pack a single coordinate. Expects `[-kWorldExtent, +kWorldExtent]`.
uint32_t packPositionComponent(float v);
float unpackPositionComponent(uint32_t q);

/// Write/read an `(x, y)` position pair as two `kPositionBits` fields.
void writePosition(BitWriter& w, float x, float y);
void readPosition(BitReader& r, float& outX, float& outY);

/// Pack a rotation in radians into `kAngleBits`. Any input is reduced
/// modulo `2*pi` first, so callers don't need to normalise.
uint32_t packAngle(float radians);
float unpackAngle(uint32_t q);

void writeAngle(BitWriter& w, float radians);
void readAngle(BitReader& r, float& outRadians);

}  // namespace engine::net
