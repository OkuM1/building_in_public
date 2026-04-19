/// @file BitStream.cpp
/// @brief Implementation of the bit-level writer/reader.
///
/// Writer model: a 64-bit `scratch_` accumulator holds up to 32 bits
/// MSB-aligned (bits arrive in the high part). `writeBits` ORs new
/// bits into the low unused region then, whenever `scratchBits_ >= 8`,
/// drains whole bytes from the high end into `buf_`.
///
/// Reader model: mirror. We track a bit position into the flat buffer
/// and extract 1..32 bits MSB-first. Past-end reads flip `ok_`.
///
/// @see include/engine/net/BitStream.h
/// @see docs/design/0008-bitstream.md

#include "engine/net/BitStream.h"

#include <cassert>
#include <cstring>

namespace engine::net {

// ---------------------------------------------------------------------------
// BitWriter
// ---------------------------------------------------------------------------

BitWriter::BitWriter() {
    buf_.reserve(64);
}

void BitWriter::writeBits(uint32_t value, int numBits) {
    assert(numBits >= 1 && numBits <= 32);
    // Mask so callers passing larger values don't corrupt the stream.
    if (numBits < 32) {
        value &= (uint32_t{1} << numBits) - 1;
    }

    // Shift incoming value up so its MSB sits just below the already-
    // accumulated bits. scratch_ layout: [ old bits | new bits | 0 pad ].
    const int shift = 64 - scratchBits_ - numBits;
    scratch_ |= (uint64_t{value} << shift);
    scratchBits_ += numBits;
    totalBits_ += static_cast<std::size_t>(numBits);

    flushScratchBytes();
}

void BitWriter::flushScratchBytes() {
    while (scratchBits_ >= 8) {
        const uint8_t byte = static_cast<uint8_t>((scratch_ >> 56) & 0xFFu);
        buf_.push_back(byte);
        scratch_ <<= 8;
        scratchBits_ -= 8;
    }
}

void BitWriter::writeBool(bool b) {
    writeBits(b ? 1u : 0u, 1);
}

void BitWriter::writeUint8(uint8_t v) {
    align();
    writeBits(v, 8);
}

void BitWriter::writeUint16(uint16_t v) {
    align();
    writeBits(v, 16);
}

void BitWriter::writeUint32(uint32_t v) {
    align();
    writeBits(v, 32);
}

void BitWriter::writeInt32(int32_t v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    writeUint32(u);
}

void BitWriter::writeFloat(float v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    writeUint32(u);
}

void BitWriter::writeVarint(uint32_t v) {
    align();
    // LEB128: 7 data bits + 1 continuation bit per byte, low-group first.
    while (v >= 0x80u) {
        writeBits(static_cast<uint32_t>((v & 0x7Fu) | 0x80u), 8);
        v >>= 7;
    }
    writeBits(v & 0x7Fu, 8);
}

void BitWriter::align() {
    const int pad = (8 - (scratchBits_ & 7)) & 7;
    if (pad > 0) {
        writeBits(0, pad);
    }
}

const std::vector<uint8_t>& BitWriter::finish() {
    align();
    return buf_;
}

// ---------------------------------------------------------------------------
// BitReader
// ---------------------------------------------------------------------------

BitReader::BitReader(const uint8_t* data, std::size_t sizeBytes)
    : data_(data), sizeBits_(sizeBytes * 8) {}

uint32_t BitReader::readBits(int numBits) {
    assert(numBits >= 1 && numBits <= 32);
    if (bitPos_ + static_cast<std::size_t>(numBits) > sizeBits_) {
        ok_ = false;
        bitPos_ = sizeBits_;  // stick at end
        return 0;
    }

    uint32_t result = 0;
    int remaining = numBits;
    while (remaining > 0) {
        const std::size_t byteIdx = bitPos_ >> 3;
        const int bitInByte = static_cast<int>(bitPos_ & 7);   // 0..7 from MSB
        const int bitsLeftInByte = 8 - bitInByte;
        const int take = remaining < bitsLeftInByte ? remaining : bitsLeftInByte;

        const uint8_t byte = data_[byteIdx];
        const int shiftDown = bitsLeftInByte - take;
        const uint32_t chunk =
            static_cast<uint32_t>((byte >> shiftDown) & ((1u << take) - 1u));

        result = (result << take) | chunk;
        bitPos_ += static_cast<std::size_t>(take);
        remaining -= take;
    }
    return result;
}

bool BitReader::readBool() {
    return readBits(1) != 0;
}

uint8_t BitReader::readUint8() {
    align();
    return static_cast<uint8_t>(readBits(8));
}

uint16_t BitReader::readUint16() {
    align();
    return static_cast<uint16_t>(readBits(16));
}

uint32_t BitReader::readUint32() {
    align();
    return readBits(32);
}

int32_t BitReader::readInt32() {
    const uint32_t u = readUint32();
    int32_t v;
    std::memcpy(&v, &u, sizeof(v));
    return v;
}

float BitReader::readFloat() {
    const uint32_t u = readUint32();
    float v;
    std::memcpy(&v, &u, sizeof(v));
    return v;
}

uint32_t BitReader::readVarint() {
    align();
    uint32_t result = 0;
    int shift = 0;
    for (int i = 0; i < 5; ++i) {  // 5 bytes cover 32 bits
        const uint32_t byte = readBits(8);
        if (!ok_) return 0;
        result |= (byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return result;
        shift += 7;
    }
    // 5 continuation bytes without a terminator is malformed input.
    ok_ = false;
    return 0;
}

void BitReader::align() {
    const std::size_t pad = (8 - (bitPos_ & 7)) & 7;
    bitPos_ += pad;
    if (bitPos_ > sizeBits_) {
        bitPos_ = sizeBits_;
        ok_ = false;
    }
}

}  // namespace engine::net
