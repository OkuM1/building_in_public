# 0011 — Delta encoding: send only what changed

- **Status:** Implemented
- **Date:** 2026-04-19
- **Phase:** 3 (Serialization & snapshots) — Commit 4/5
- **Related:** [0010](0010-snapshot-format.md)

## Context

Full snapshots (Commit 3) cost ~5 bytes per `Transform` plus per-entity
overhead. At 1000 entities × 20 Hz that's ~100 KB/s per client —
enough to blow the < 32 KB/s budget by 3×. But the observed change
per tick in a typical game is small: a handful of entities moved,
most didn't.

Delta encoding ships only the difference between two successive
snapshots. An unmoving entity costs zero bytes. Rare case (lots of
movement) degrades gracefully to the full-snapshot cost plus a small
header.

## Decision

### Added trait method: `Serializer<T>::equal(const T&, const T&)`

Each component serializer must now provide an equality predicate.
For quantised types the predicate compares *quantised representations*
— `Transform::equal` calls `packPositionComponent` on both sides and
compares codes, not raw floats. Without this, sub-quantum float drift
(e.g., `1.0` vs `1.0 + 1e-7`) would flag every entity as changed on
every tick and destroy the whole point of delta encoding.

### Added functions: `encodeDelta<List>` / `applyDelta<List>`

```cpp
template <typename List>
void encodeDelta(World& baseline, World& current,
                 uint32_t tick, uint32_t baselineTick,
                 BitWriter& out);

template <typename List>
bool applyDelta(BitReader& in, World& target,
                uint32_t& outTick, uint32_t& outBaselineTick);
```

### Wire format

```
delta :=
    varint tick
    varint baseline_tick
    varint changed_count
    {varint id, uint8 mask, components_in_mask_order} * changed_count
    varint removed_count
    {varint id} * removed_count
```

The `changed` entry format is *identical* to a full-snapshot entity
entry. The only semantic difference is that `mask` now means
"components that changed since baseline" instead of "components
present". This intentionally allows a "new entity" (absent in
baseline, present in current) to be expressed with the same bits —
the receiver sees a mask with some bits set, reads the payload,
and adds each component whether or not the entity had it before.

### Changed-bit rule

A bit is set in the changed mask iff:

1. The component is **present in `current`**, AND
2. Either it is absent in `baseline`, OR its `equal(base, cur)` is
   false.

The rule is asymmetric on purpose: the sender must be able to emit
a payload for every set bit, and it can only emit a payload for
components that actually exist in `current`.

### Partial component removal: not handled in v1

If an entity has `Transform + Velocity` in baseline and only
`Transform` in current (Velocity was removed but the entity still
exists), the v1 delta format cannot express this. The entity appears
unchanged and the receiver's `Velocity` stays. Full-entity removal
(the replicated set became empty) *is* handled via the removed-ids
list. Partial component removal is noted as a known limitation; a
plausible fix is a second mask byte ("cleared-components mask") per
changed entry, deferred until a gameplay need surfaces.

## Alternatives considered

1. **Baseline-less delta (diff against the previous tick implicitly).**
   Simpler on the wire but fragile: one dropped packet and the
   receiver's state diverges forever. Rejected — we want explicit
   baselines (the ack scheme in Phase 4 keys on them).
2. **Per-field (not per-component) deltas.** E.g., send only `x` if
   only `x` changed. Extra bookkeeping, small bandwidth win
   (~10–15%), big complexity cost. Rejected for now; revisit if
   benchmarks call for it.
3. **Client computes diff from last received snapshot** rather than
   trusting server-side baseline tracking. Simpler server but can't
   express "entity removed". Rejected.
4. **Bitset-indexed "which entities changed"** upfront instead of a
   linear `changed_count` + per-entity id. Saves ~0–1 byte per
   changed entity but adds MAX_ENTITIES/8 bytes of overhead — wrong
   trade-off until MAX_ENTITIES shrinks or the change rate becomes
   very high.

## Consequences

- **Enables** the bandwidth target from the roadmap (typical delta
  < 200 B with most entities idle). Commit 5 measures this.
- **Enables** Phase 4's reliable-UDP ack scheme: the server keeps a
  ring buffer of recent snapshots, the client acks the most recent
  it received, and the next delta is encoded against that baseline.
- **Requires** that `World::destroyEntity` be idempotent-ish — the
  receiver calls it on every removed id regardless of current state.
  The current implementation is fine because it just resets
  signatures and pushes the id back to the available queue.

## Code pointers

- [include/engine/net/Serialize.h](../../include/engine/net/Serialize.h)
  — `encodeDelta` / `applyDelta`, plus `maybeDiffOne`
- [include/game/components/ComponentSerializers.h](../../include/game/components/ComponentSerializers.h)
  — `equal` methods added to all three component serializers
- [tests/test_delta.cpp](../../tests/test_delta.cpp) — 6 cases, 19
  assertions; covers identical-worlds (4-byte delta), single-field
  change, entity add, entity remove, apply-then-equal, sub-quantum
  drift suppression.

## Open questions

- **Baseline storage on the server.** Not part of this commit. The
  server will need a ring buffer of past worlds (or past snapshots)
  keyed by tick. Ship in Phase 5.
- **Entity id stability across server restarts.** Same note as 0010.
- **Partial component removal.** See above. Add a cleared-mask byte
  if a gameplay feature ever needs it.
