/// @file test_bitstream.cpp
/// @brief Unit tests for `engine::net::BitStream`.
///
/// Covers the cases that matter for the Phase 3 snapshot work:
///   - Roundtrip of arbitrary bit widths (1, 7, 13, 32).
///   - Sub-byte reads straddling byte boundaries.
///   - Typed writes (bool, u8, u16, u32, i32, float) roundtrip.
///   - Varint encoding length matches the LEB128 contract.
///   - Read-past-end flips `ok()` to false without crashing.
///
/// @see docs/design/0008-bitstream.md
/// @see docs/learning/0008-wire-format-discipline.md

#include <doctest/doctest.h>

#include <cstdint>
#include <limits>

#include "engine/net/BitStream.h"

using engine::net::BitReader;
using engine::net::BitWriter;

TEST_CASE("BitWriter + BitReader roundtrip arbitrary bit widths") {
    BitWriter w;
    w.writeBits(0b1u, 1);
    w.writeBits(0b1010101u, 7);
    w.writeBits(0b1111000011110000u, 16);
    w.writeBits(0x12345678u, 32);
    w.writeBits(0b101u, 3);  // tail that doesn't fall on a byte boundary

    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());

    CHECK(r.readBits(1) == 0b1u);
    CHECK(r.readBits(7) == 0b1010101u);
    CHECK(r.readBits(16) == 0b1111000011110000u);
    CHECK(r.readBits(32) == 0x12345678u);
    CHECK(r.readBits(3) == 0b101u);
    CHECK(r.ok());
}

TEST_CASE("Typed writes roundtrip") {
    BitWriter w;
    w.writeBool(true);
    w.writeBool(false);
    w.writeUint8(0xAB);
    w.writeUint16(0xBEEF);
    w.writeUint32(0xDEADBEEFu);
    w.writeInt32(-123456);
    w.writeFloat(3.14159f);

    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());

    CHECK(r.readBool() == true);
    CHECK(r.readBool() == false);
    CHECK(r.readUint8() == 0xAB);
    CHECK(r.readUint16() == 0xBEEF);
    CHECK(r.readUint32() == 0xDEADBEEFu);
    CHECK(r.readInt32() == -123456);
    CHECK(r.readFloat() == doctest::Approx(3.14159f));
    CHECK(r.ok());
}

TEST_CASE("Varint encodes small values in 1 byte, large in 5") {
    {
        BitWriter w;
        w.writeVarint(0);
        w.writeVarint(1);
        w.writeVarint(127);
        // 3 one-byte varints = 3 bytes total.
        CHECK(w.finish().size() == 3u);
    }
    {
        BitWriter w;
        w.writeVarint(128);     // just over 1-byte boundary => 2 bytes
        CHECK(w.finish().size() == 2u);
    }
    {
        BitWriter w;
        w.writeVarint(std::numeric_limits<uint32_t>::max());  // 5 bytes
        CHECK(w.finish().size() == 5u);
    }
}

TEST_CASE("Varint roundtrip across the full range") {
    const uint32_t values[] = {
        0u, 1u, 127u, 128u, 16383u, 16384u,
        1u << 20, 1u << 28,
        std::numeric_limits<uint32_t>::max(),
    };

    BitWriter w;
    for (uint32_t v : values) w.writeVarint(v);

    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());
    for (uint32_t expected : values) {
        CHECK(r.readVarint() == expected);
    }
    CHECK(r.ok());
}

TEST_CASE("Read past end flips ok() to false") {
    BitWriter w;
    w.writeUint8(0x42);
    const auto& bytes = w.finish();

    BitReader r(bytes.data(), bytes.size());
    CHECK(r.readUint8() == 0x42);
    CHECK(r.ok());

    // One more read: buffer is exhausted.
    (void)r.readUint8();
    CHECK_FALSE(r.ok());
}

TEST_CASE("Interleaved bit + byte writes respect alignment") {
    BitWriter w;
    w.writeBits(0b101u, 3);
    w.writeUint8(0xAA);   // must auto-align; the 5 pad bits are zeroed
    w.writeBits(0b11u, 2);

    const auto& bytes = w.finish();
    BitReader r(bytes.data(), bytes.size());
    CHECK(r.readBits(3) == 0b101u);
    CHECK(r.readUint8() == 0xAA);
    CHECK(r.readBits(2) == 0b11u);
    CHECK(r.ok());
}
