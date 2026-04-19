/// @file ComponentSerializers.h
/// @brief `engine::net::Serializer<T>` specialisations for `game::` components.
///
/// Having these live in `game/` (not `engine/`) is deliberate: the
/// engine's snapshot machinery is component-agnostic, so the mapping
/// from a concrete game component to bits on the wire is the game's
/// responsibility. Include this header once (typically in the TU that
/// sets up the world and calls `encodeSnapshot`) to register all
/// specialisations visible to the compiler.
///
/// @par Wire format for each component
/// - `Transform`      → 16 bits x, 16 bits y, 8 bits rotation (40 bits)
/// - `Velocity`       → 32 bits vx, 32 bits vy (raw float; quantise later)
/// - `PlayerInput`    → 6 bits (one per action) (byte-aligned on next write)
///
/// The `Velocity` format is intentionally sub-optimal: we don't know
/// the expected velocity range yet. Revisit once physics land in
/// Phase 5. Raw floats round-trip exactly, which is right for now.
///
/// @see include/engine/net/Serialize.h
/// @see docs/design/0010-snapshot-format.md
#pragma once

#include "engine/net/BitStream.h"
#include "engine/net/Quantize.h"
#include "engine/net/Serialize.h"
#include "game/components/GameComponents.h"

namespace engine::net {

// ---------------------------------------------------------------------------
// Transform: position (quantised) + rotation (quantised)
// ---------------------------------------------------------------------------

template <>
struct Serializer<game::Transform> {
    static void write(BitWriter& out, const game::Transform& t) {
        writePosition(out, t.x, t.y);
        writeAngle(out, t.rotation);
    }

    static void read(BitReader& in, game::Transform& t) {
        readPosition(in, t.x, t.y);
        readAngle(in, t.rotation);
    }

    /// Compare at quantised resolution. Two Transforms that round to
    /// the same codes on the wire are "equal" for delta purposes,
    /// even if their raw floats differ sub-quantum. Without this,
    /// every tick's float drift would flag every entity as changed.
    static bool equal(const game::Transform& a, const game::Transform& b) {
        return packPositionComponent(a.x) == packPositionComponent(b.x)
            && packPositionComponent(a.y) == packPositionComponent(b.y)
            && packAngle(a.rotation)      == packAngle(b.rotation);
    }
};

// ---------------------------------------------------------------------------
// Velocity: raw floats (placeholder — quantise in Phase 5 once range known)
// ---------------------------------------------------------------------------

template <>
struct Serializer<game::Velocity> {
    static void write(BitWriter& out, const game::Velocity& v) {
        out.writeFloat(v.vx);
        out.writeFloat(v.vy);
    }

    static void read(BitReader& in, game::Velocity& v) {
        v.vx = in.readFloat();
        v.vy = in.readFloat();
    }

    /// Raw-float equality: matches the raw-float wire format. Switch to
    /// quantised comparison when `Velocity` is quantised in Phase 5.
    static bool equal(const game::Velocity& a, const game::Velocity& b) {
        return a.vx == b.vx && a.vy == b.vy;
    }
};

// ---------------------------------------------------------------------------
// PlayerInput: 6 bools packed into 6 bits
// ---------------------------------------------------------------------------

template <>
struct Serializer<game::PlayerInput> {
    static void write(BitWriter& out, const game::PlayerInput& p) {
        out.writeBool(p.moveUp);
        out.writeBool(p.moveDown);
        out.writeBool(p.moveLeft);
        out.writeBool(p.moveRight);
        out.writeBool(p.attack);
        out.writeBool(p.dodge);
    }

    static void read(BitReader& in, game::PlayerInput& p) {
        p.moveUp    = in.readBool();
        p.moveDown  = in.readBool();
        p.moveLeft  = in.readBool();
        p.moveRight = in.readBool();
        p.attack    = in.readBool();
        p.dodge     = in.readBool();
    }

    static bool equal(const game::PlayerInput& a, const game::PlayerInput& b) {
        return a.moveUp    == b.moveUp
            && a.moveDown  == b.moveDown
            && a.moveLeft  == b.moveLeft
            && a.moveRight == b.moveRight
            && a.attack    == b.attack
            && a.dodge     == b.dodge;
    }
};

// ---------------------------------------------------------------------------
// The canonical replication list for Sumo Arena / the demo client.
//
// Position in this list == bit in the on-wire entity component mask.
// Do not reorder without bumping the wire format version.
// ---------------------------------------------------------------------------

using GameReplication = ReplicationList<
    game::Transform,     // bit 0
    game::Velocity,      // bit 1
    game::PlayerInput    // bit 2
>;

}  // namespace engine::net
