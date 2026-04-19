# 0010 — Snapshot wire format v1 (trait-based encoder)

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 3 (Serialization & snapshots) — Commit 3/5
- **Related:** [0008](0008-bitstream.md), [0009](0009-quantization.md)

## Context

Commit 3 turns the byte-level tools (`BitStream`, `Quantize`) into a
full-world serializer: `encodeSnapshot(World) → bytes` and its
inverse. Every later Phase 3 commit (delta encoding, benchmarks) and
everything in Phases 4–5 (UDP, replication) consumes this format.

Two orthogonal decisions needed making:

1. **Code organisation.** How does the encoder know about components?
2. **Wire format.** What bytes actually go out?

## Decision

### (1) Trait-based extension point

A primary template `engine::net::Serializer<T>` with no body. Games
specialise it per component type. A `ReplicationList<Components...>`
names the replicated types in a fixed order, and the top-level
functions are template functions parametrised by that list:

```cpp
template <typename List>
void encodeSnapshot(World&, uint32_t tick, BitWriter&);

template <typename List>
bool decodeSnapshot(BitReader&, World&, uint32_t& outTick);
```

`engine::net::Serialize.h` provides the *mechanism* and knows zero
component types. `game/components/ComponentSerializers.h` provides
the *policy*: `Serializer<game::Transform>`, `Serializer<game::Velocity>`,
`Serializer<game::PlayerInput>`, and the canonical alias
`GameReplication = ReplicationList<Transform, Velocity, PlayerInput>`.

New component in six months: add a specialisation, add it to the list,
done. `Snapshot.cpp` (doesn't exist) isn't touched because the
dispatch is compile-time via C++17 fold expressions over the pack.

### (2) Wire format v1

```
snapshot :=
    varint tick
    varint entity_count
    entity * entity_count

entity :=
    varint entity_id
    uint8  component_mask      // bit N <=> List's Nth type present
    (components in bit-index order, each per its Serializer<T>)
```

Concrete sizes (encoded by Commit 2's quantiser):

| Component | Bits |
|---|---|
| Transform (pos_x, pos_y, rotation) | 16+16+8 = 40 |
| Velocity (raw floats, placeholder) | 32+32 = 64 |
| PlayerInput (6 booleans) | 6 |

Per-entity overhead: varint id (typically 1 byte) + 1-byte mask = ~2
bytes. A single-Transform entity ships in ~7 bytes.

**8-component ceiling** on `ReplicationList` size (one-byte mask). A
`static_assert` in `ReplicationList` pins this; widening to u16 or
varint is a wire bump.

## Alternatives considered

1. **Monolithic `encodeSnapshot(World&)` with hard-coded component
   branches.** Simpler today; every added component touches one file.
   Rejected at the user's direction — the project is explicitly
   planning to add more components, and the trait path scales without
   a rewrite.
2. **Runtime component registry** (`registerComponent<T>(writer_fn,
   reader_fn)` at startup). More flexible (can add components via
   config) but loses compile-time guarantees: a missing specialisation
   becomes a runtime crash instead of a compile error. Rejected —
   netcode errors should be build errors.
3. **Protobuf / Cap'n Proto schema.** Rejected previously (see 0008).
   Revisiting would mean throwing out both `BitStream` and `Quantize`.
4. **Skip the per-entity mask; fix a schema per snapshot.** Would save
   1 byte/entity (~1 KB for 1k entities). Rejected for this commit
   because it tangles component presence with snapshot format. Delta
   encoding (Commit 4) will make the savings marginal anyway.
5. **Delta-encoded entity IDs** (varint of `id - prev_id`). Would save
   ~0–1 byte/entity. Deferred — not on the critical path for Commit 3
   correctness. Revisit in Commit 4 or 5 if the benchmark needs it.

## Consequences

- **Enables** Commit 4 (delta encoding): each `Serializer<T>` can
  grow a third `diff(BitWriter&, old, new) → bool` method without
  touching the top-level encoder.
- **Enables** testing: `encode → decode` is a pure function pair over
  `World`, trivial to unit test.
- **Accepts** an 8-component hard cap; if the project crosses that
  line, widen the mask and bump the format version.
- **Accepts** that decoding into an unknown entity id is a **no-op**
  on the receiver's world for now. Spawn-on-receive is Phase 5.

## Code pointers

- [include/engine/net/Serialize.h](../../include/engine/net/Serialize.h)
- [include/game/components/ComponentSerializers.h](../../include/game/components/ComponentSerializers.h)
- [tests/test_snapshot.cpp](../../tests/test_snapshot.cpp) — 6 test
  cases, 37 assertions, covering empty world, full roundtrip, partial
  masks, multi-entity, no-replicated-components, and truncated input.

## Open questions

- **Spawn-on-receive.** The decoder currently asserts the entity id
  is in range but doesn't create missing entities. When Phase 5's
  replication model lands, spawn events will likely live on a
  separate reliable channel rather than inside the snapshot.
- **Entity id stability.** Works today because both worlds allocate
  ids in the same order from a fresh state. The moment the server
  starts destroying entities, client-side ids diverge. This is the
  problem Phase 5's entity-replication layer exists to solve — not
  something Phase 3's snapshot format should try to.
- **Velocity encoding.** Currently raw floats (64 bits total).
  Target: 16 bits/axis once max velocity is known from Sumo Arena
  physics. Placeholder on purpose.
