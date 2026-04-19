/// @file Serialize.h
/// @brief Snapshot serialization: trait + top-level encode/decode.
///
/// The engine supplies the *mechanism*:
///
///   - A primary template `Serializer<T>` that games specialise to tell
///     the engine how to pack a component type into bits.
///   - A compile-time list `ReplicationList<Components...>` that names
///     the components eligible for replication, in a fixed order that
///     becomes the on-wire component-mask bit order.
///   - Two top-level functions, `encodeSnapshot<List>` and
///     `decodeSnapshot<List>`, that walk the world and dispatch to the
///     per-component `Serializer<T>`s.
///
/// The engine does **not** know about any specific component type. The
/// game (or a testbed, or a unit test) instantiates the template with
/// its own `ReplicationList`. This keeps `engine::` headless-safe
/// *and* free of game-specific symbols.
///
/// @par Wire format v1
/// @code
/// snapshot :=
///     varint  tick
///     varint  entity_count
///     entity * entity_count
///
/// entity :=
///     varint  entity_id
///     uint8   component_mask       // bit N <=> List's component N present
///     (each present component packed in bit-index order)
/// @endcode
///
/// Max 8 replicated components per `ReplicationList` (1-byte mask). Can
/// be widened to 16/32 later; that's a wire bump.
///
/// @see docs/design/0010-snapshot-format.md
/// @see docs/learning/0010-traits-over-branches.md
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "engine/ecs/World.h"
#include "engine/net/BitStream.h"

namespace engine::net {

// ---------------------------------------------------------------------------
// Serializer<T>: the single extension point.
//
// Game code specialises this for each replicable component type. A
// specialisation must define three static members:
//
//     static void write(BitWriter& out, const T& value);
//     static void read (BitReader& in,        T& value);
//     static bool equal(const T& a, const T& b);
//
// `equal` answers the question "would these two values produce the
// same bytes on the wire?". For quantised types it should compare the
// quantised representations, not raw floats — otherwise tiny
// sub-quantum drift would flag every tick as a change and defeat
// delta encoding entirely.
//
// The primary template is intentionally undefined so that attempting to
// replicate a component with no specialisation fails at compile time
// with a clear "incomplete type" error instead of silently at runtime.
// ---------------------------------------------------------------------------

template <typename T>
struct Serializer;

// ---------------------------------------------------------------------------
// ReplicationList: compile-time, ordered list of component types.
//
// The *position* of a component in the list becomes its bit in the
// per-entity component mask on the wire. Reordering the list is a wire
// format break.
// ---------------------------------------------------------------------------

template <typename... Components>
struct ReplicationList {
    static constexpr std::size_t count = sizeof...(Components);
    static_assert(count <= 8,
                  "ReplicationList currently limited to 8 components "
                  "(1-byte mask on the wire). Widen the mask to extend.");
};

// ---------------------------------------------------------------------------
// Implementation detail: per-component helpers parametrised by both the
// component type and its bit index in the mask. C++17 fold expressions
// are used to unroll over the ReplicationList in a single statement.
// ---------------------------------------------------------------------------

namespace detail {

template <std::size_t Bit, typename C>
inline void maybeWriteOne(World& w, EntityId e, uint8_t mask, BitWriter& out) {
    if (mask & (1u << Bit)) {
        Serializer<C>::write(out, w.getComponent<C>(e));
    }
}

template <std::size_t Bit, typename C>
inline uint8_t maskBitIfPresent(World& w, EntityId e) {
    return w.hasComponent<C>(e) ? static_cast<uint8_t>(1u << Bit) : 0u;
}

template <std::size_t Bit, typename C>
inline void maybeReadOne(BitReader& in, World& w, EntityId e, uint8_t mask) {
    if ((mask & (1u << Bit)) == 0) return;

    C value{};
    Serializer<C>::read(in, value);
    if (w.hasComponent<C>(e)) {
        w.getComponent<C>(e) = value;
    } else {
        w.addComponent<C>(e, value);
    }
}

template <typename... Cs, std::size_t... Is>
inline uint8_t computeMaskImpl(World& w, EntityId e,
                               ReplicationList<Cs...>,
                               std::index_sequence<Is...>) {
    uint8_t mask = 0;
    ((mask |= maskBitIfPresent<Is, Cs>(w, e)), ...);
    return mask;
}

template <typename... Cs, std::size_t... Is>
inline void writeComponentsImpl(World& w, EntityId e, uint8_t mask,
                                BitWriter& out,
                                ReplicationList<Cs...>,
                                std::index_sequence<Is...>) {
    (maybeWriteOne<Is, Cs>(w, e, mask, out), ...);
}

template <typename... Cs, std::size_t... Is>
inline void readComponentsImpl(BitReader& in, World& w, EntityId e,
                               uint8_t mask,
                               ReplicationList<Cs...>,
                               std::index_sequence<Is...>) {
    (maybeReadOne<Is, Cs>(in, w, e, mask), ...);
}

// ---------------------------------------------------------------------------
// Delta helpers. `maybeDiffOne` sets bit `Bit` in `outMask` if and
// only if the component is **present in current** and either absent
// in baseline or differs in value. Components present in baseline
// but absent in current are handled by the removed-ids list (when
// the whole entity's replicated set vanished) or, for partial
// component removal, are a limitation of the v1 delta format —
// future work.
// ---------------------------------------------------------------------------

template <std::size_t Bit, typename C>
inline void maybeDiffOne(World& base, World& cur, EntityId e, uint8_t& outMask) {
    if (!cur.hasComponent<C>(e)) return;
    if (!base.hasComponent<C>(e)) {
        outMask |= static_cast<uint8_t>(1u << Bit);
        return;
    }
    if (!Serializer<C>::equal(base.getComponent<C>(e), cur.getComponent<C>(e))) {
        outMask |= static_cast<uint8_t>(1u << Bit);
    }
}

template <typename... Cs, std::size_t... Is>
inline uint8_t computeDiffMaskImpl(World& base, World& cur, EntityId e,
                                   ReplicationList<Cs...>,
                                   std::index_sequence<Is...>) {
    uint8_t mask = 0;
    ((maybeDiffOne<Is, Cs>(base, cur, e, mask)), ...);
    return mask;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Public snapshot API.
// ---------------------------------------------------------------------------

/// Encode every live entity in `world` that carries at least one
/// component from `List` into `out`. The `tick` header is written
/// first so the decoder can place the snapshot on its own timeline.
///
/// @param world   source world (non-const: `World::hasComponent` is not const)
/// @param tick    simulation tick this snapshot represents
/// @param out     bit writer receiving the encoded bytes
/// @tparam List   `ReplicationList<...>` specifying replicated types
template <typename List>
void encodeSnapshot(World& world, uint32_t tick, BitWriter& out) {
    using Seq = std::make_index_sequence<List::count>;

    // Pass 1: count entities that contribute at least one bit to the mask.
    // (MAX_ENTITIES is small — 10k — and this is the straightforward
    // approach. A later optimisation can cache the entity set.)
    uint32_t liveCount = 0;
    for (EntityId e = 0; e < MAX_ENTITIES; ++e) {
        const uint8_t mask = detail::computeMaskImpl(world, e, List{}, Seq{});
        if (mask != 0) ++liveCount;
    }

    out.writeVarint(tick);
    out.writeVarint(liveCount);

    // Pass 2: emit each qualifying entity.
    for (EntityId e = 0; e < MAX_ENTITIES; ++e) {
        const uint8_t mask = detail::computeMaskImpl(world, e, List{}, Seq{});
        if (mask == 0) continue;

        out.writeVarint(static_cast<uint32_t>(e));
        out.writeUint8(mask);
        detail::writeComponentsImpl(world, e, mask, out, List{}, Seq{});
    }
}

/// Decode a snapshot previously produced by `encodeSnapshot<List>`.
///
/// Assumes entity IDs referenced in the snapshot already exist in
/// `world`. If an entity id is unknown, its components are still
/// *read* from the stream (so the cursor stays aligned) but discarded.
/// Spawn-on-receive is a Phase 5 concern.
///
/// @param in       bit reader positioned at the snapshot start
/// @param world    target world (pre-populated with matching entity ids)
/// @param outTick  receives the tick number from the snapshot header
/// @return `true` on a well-formed snapshot, `false` if the stream ran
///         out or was malformed
template <typename List>
bool decodeSnapshot(BitReader& in, World& world, uint32_t& outTick) {
    using Seq = std::make_index_sequence<List::count>;

    outTick = in.readVarint();
    const uint32_t count = in.readVarint();
    if (!in.ok()) return false;

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t id = in.readVarint();
        const uint8_t mask = in.readUint8();
        if (!in.ok()) return false;
        assert(id < MAX_ENTITIES && "snapshot references out-of-range entity id");

        const EntityId e = static_cast<EntityId>(id);
        detail::readComponentsImpl(in, world, e, mask, List{}, Seq{});
    }

    return in.ok();
}

// ---------------------------------------------------------------------------
// Delta snapshots.
//
// A delta expresses `current - baseline`: only entities whose
// component set or values differ are sent, and only the components
// that actually differ within those entities. Entities present in
// `baseline` but absent from `current` appear in a separate
// removed-ids list so the receiver can destroy them.
//
// @par Wire format
// @code
/// delta :=
///     varint tick
///     varint baseline_tick
///     varint changed_count
///     changed_entry * changed_count
///     varint removed_count
///     varint * removed_count              // entity ids to destroy
///
/// changed_entry :=
///     varint entity_id
///     uint8  mask                         // bits set <=> components in payload
///     (components in bit-index order; values are the *current* ones)
/// @endcode
// ---------------------------------------------------------------------------

/// Encode `current - baseline` into `out`. Both worlds must have the
/// same registered components. An entity present on both sides with
/// identical replicated state contributes zero bytes.
///
/// @param baseline       last snapshot the receiver acknowledged
/// @param current        latest authoritative world state
/// @param tick           current tick
/// @param baselineTick   tick of `baseline` (echoed on the wire so the
///                       receiver can sanity-check; also useful for
///                       logging/telemetry)
/// @param out            bit writer receiving the encoded bytes
template <typename List>
void encodeDelta(World& baseline, World& current,
                 uint32_t tick, uint32_t baselineTick,
                 BitWriter& out) {
    using Seq = std::make_index_sequence<List::count>;

    // Pass 1: count changed entities and removed entities.
    uint32_t changedCount = 0;
    uint32_t removedCount = 0;
    for (EntityId e = 0; e < MAX_ENTITIES; ++e) {
        const uint8_t diffMask =
            detail::computeDiffMaskImpl(baseline, current, e, List{}, Seq{});
        if (diffMask != 0) ++changedCount;

        // "Removed" = was replicated in baseline, contributes nothing in current.
        const uint8_t baseMask =
            detail::computeMaskImpl(baseline, e, List{}, Seq{});
        const uint8_t curMask =
            detail::computeMaskImpl(current, e, List{}, Seq{});
        if (baseMask != 0 && curMask == 0) ++removedCount;
    }

    out.writeVarint(tick);
    out.writeVarint(baselineTick);
    out.writeVarint(changedCount);

    // Pass 2: emit changed entities with only the differing components.
    for (EntityId e = 0; e < MAX_ENTITIES; ++e) {
        const uint8_t diffMask =
            detail::computeDiffMaskImpl(baseline, current, e, List{}, Seq{});
        if (diffMask == 0) continue;

        out.writeVarint(static_cast<uint32_t>(e));
        out.writeUint8(diffMask);
        // For each bit set in diffMask we write the *current* component
        // value. `writeComponentsImpl` reads from `current` and only
        // writes components whose bit is set.
        detail::writeComponentsImpl(current, e, diffMask, out, List{}, Seq{});
    }

    out.writeVarint(removedCount);
    for (EntityId e = 0; e < MAX_ENTITIES; ++e) {
        const uint8_t baseMask =
            detail::computeMaskImpl(baseline, e, List{}, Seq{});
        const uint8_t curMask =
            detail::computeMaskImpl(current, e, List{}, Seq{});
        if (baseMask != 0 && curMask == 0) {
            out.writeVarint(static_cast<uint32_t>(e));
        }
    }
}

/// Apply a previously-encoded delta to `target` in-place. Changed
/// components are overwritten (or added); removed entity ids have
/// their replicated components dropped (the entity itself is
/// destroyed via `World::destroyEntity`).
///
/// @param in              bit reader positioned at the delta start
/// @param target          receiver world (holding a state equal to the
///                        baseline the sender used)
/// @param outTick         receives the `tick` header from the delta
/// @param outBaselineTick receives the `baseline_tick` header
/// @return `true` if the delta decoded cleanly
template <typename List>
bool applyDelta(BitReader& in, World& target,
                uint32_t& outTick, uint32_t& outBaselineTick) {
    using Seq = std::make_index_sequence<List::count>;

    outTick = in.readVarint();
    outBaselineTick = in.readVarint();
    const uint32_t changedCount = in.readVarint();
    if (!in.ok()) return false;

    for (uint32_t i = 0; i < changedCount; ++i) {
        const uint32_t id = in.readVarint();
        const uint8_t mask = in.readUint8();
        if (!in.ok()) return false;
        assert(id < MAX_ENTITIES && "delta references out-of-range entity id");

        const EntityId e = static_cast<EntityId>(id);
        detail::readComponentsImpl(in, target, e, mask, List{}, Seq{});
    }

    const uint32_t removedCount = in.readVarint();
    if (!in.ok()) return false;
    for (uint32_t i = 0; i < removedCount; ++i) {
        const uint32_t id = in.readVarint();
        if (!in.ok()) return false;
        assert(id < MAX_ENTITIES && "delta references out-of-range entity id");
        target.destroyEntity(static_cast<EntityId>(id));
    }

    return in.ok();
}

}  // namespace engine::net
