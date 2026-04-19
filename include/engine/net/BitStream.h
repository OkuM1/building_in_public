/// @file BitStream.h
/// @brief Bit-level serialization primitives.
///
/// `BitWriter` appends 1–32 bits at a time to a growing byte buffer,
/// MSB-first within each byte (network-standard order). `BitReader`
/// consumes the same format. Variable-length integers (LEB128) are
/// byte-aligned and use 7 data bits + 1 continuation bit per byte.
///
/// These are the foundation for everything in Phase 3: quantised
/// component serializers, world snapshots, and delta encoding all sit
/// on top of this file.
///
/// @note Writer never bounds-checks — the buffer grows. Reader flips
///       an internal `ok` flag on read-past-end and subsequent reads
///       return 0. Check `ok()` after a decode batch.
///
/// @see docs/design/0008-bitstream.md
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::net {

/// Bit-level writer that appends to a `std::vector<uint8_t>`.
///
/// The writer owns an internal accumulator of up to 8 unflushed bits.
/// `flush()` pads the accumulator to the next byte boundary with zeros.
/// Varint and typed writes (`writeUint16`, `writeFloat`, ...) that
/// logically operate on bytes call `align()` internally so they always
/// land on byte boundaries in the output.
class BitWriter {
public:
    BitWriter();

    /// Append the low `numBits` bits of `value`, MSB-first.
    /// @param value the value to write; only the low `numBits` bits are used.
    /// @param numBits in [1, 32].
    void writeBits(uint32_t value, int numBits);

    void writeBool(bool b);
    void writeUint8(uint8_t v);
    void writeUint16(uint16_t v);
    void writeUint32(uint32_t v);
    void writeInt32(int32_t v);

    /// Writes the 32 raw bits of `v`. Platform endianness is assumed to
    /// match between sender and receiver (Phase 3 is in-process; a
    /// cross-host fix lands in Phase 4).
    void writeFloat(float v);

    /// LEB128-style varint: 7 data bits + 1 continuation bit per byte.
    /// Byte-aligned. Typical snapshot sizes: 1 byte for ids < 128.
    void writeVarint(uint32_t v);

    /// Pad the bit accumulator to the next byte boundary with zeros.
    void align();

    /// Pad and return the final byte buffer. After calling, the writer
    /// is still usable but the returned reference is stable as long as
    /// no further writes occur.
    const std::vector<uint8_t>& finish();

    std::size_t bitsWritten() const { return totalBits_; }
    std::size_t bytesWritten() const { return (totalBits_ + 7) / 8; }

private:
    std::vector<uint8_t> buf_;
    uint64_t scratch_ = 0;   ///< bits not yet flushed, held MSB-aligned
    int scratchBits_ = 0;    ///< count of valid bits in scratch_ (0..32)
    std::size_t totalBits_ = 0;

    void flushScratchBytes();
};

/// Bit-level reader over an externally-owned byte buffer.
///
/// On read-past-end the reader sets an internal error flag; further
/// reads return 0 and `ok()` stays false. This keeps decode paths
/// branch-light — callers check `ok()` once at the end of a snapshot.
class BitReader {
public:
    BitReader(const uint8_t* data, std::size_t sizeBytes);

    uint32_t readBits(int numBits);
    bool readBool();
    uint8_t readUint8();
    uint16_t readUint16();
    uint32_t readUint32();
    int32_t readInt32();
    float readFloat();
    uint32_t readVarint();

    /// Skip to the next byte boundary.
    void align();

    bool ok() const { return ok_; }
    std::size_t bitsRead() const { return bitPos_; }
    std::size_t bytesRead() const { return (bitPos_ + 7) / 8; }

private:
    const uint8_t* data_;
    std::size_t sizeBits_;
    std::size_t bitPos_ = 0;
    bool ok_ = true;
};

}  // namespace engine::net
