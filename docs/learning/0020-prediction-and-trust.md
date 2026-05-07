# 0020 — Prediction and Trust: Why the Client Never Believes Itself

> **Phase:** 5a
> **Topic:** Client-side prediction, snapshot interpolation, and the reconciliation mental model

---

## The problem in one sentence

You want the player's input to feel instant, but the server is 50–150 ms away
and it *owns the truth*.

---

## What prediction actually is

When you press the right arrow key, the client immediately moves your character.
It doesn't wait for the server to confirm. This is **client-side prediction**:
the client runs the same simulation logic as the server and guesses what the
authoritative result will be.

This guess is *usually* right. It's wrong when:

- Your input packet is lost (the server never saw it).
- Another player interacted with you (the server applied a collision you didn't
  model locally because you didn't know where the other player was yet).
- The simulation has floating-point divergence (rare with deterministic physics;
  less rare with non-deterministic physics).

The key insight: **the client must never treat its predicted state as fact**.
Every predicted transform is provisional, stamped with the tick it was
predicted at, and held in a ring buffer until the server confirms it.

---

## The snapshot buffer and the render delay

The server sends world snapshots at 20 Hz. If the client renders at the
server's "now" it must extrapolate the future, which falls apart under jitter.

The solution is a **render delay**: the client buffers 2–3 snapshots and
renders the *past*, not the present. At a 100 ms render delay the client
always has two bracketing snapshots to lerp between. The visual result is
smooth. The perceptual cost is ~2 frames of added latency for *remote*
entities. The local player is exempt — they run prediction.

The `SnapshotBuffer` handles this: push snapshots as they arrive; call
`interpolate(entityId, renderTick)` to get a linearly blended `Transform`
between the two adjacent snapshots.

---

## Why "snap and replay" for reconciliation

When the server sends an authoritative snapshot at tick T, the client checks:
does my predicted transform at T match the server's?

If they match: great. Discard confirmed entries via `ackUpTo(T)`.

If they don't match ("misprediction"): the client **snaps** to the server's
authoritative state at T and **replays** every unconfirmed input from T+1
forward. The new predicted transforms replace the old ones.

Why replay, not just snap to the server state?

Because the server's snapshot is already in the *past* by the time it arrives.
If you just applied it, the client would visually rubber-band back in time.
Replay brings the prediction back to "now" by re-applying everything the player
did after tick T, producing a present-moment estimate that incorporates the
server's correction.

---

## The two timelines

A useful mental model: the client runs **two timelines** simultaneously.

| Timeline | What it shows | Data source |
|---|---|---|
| **Prediction timeline** (now) | Local player only, immediate | PredictionBuffer |
| **Interpolation timeline** (past) | All remote entities, delayed | SnapshotBuffer |

The local player jumps between these timelines seamlessly. The renderer draws
the prediction timeline position for the local player and the interpolation
timeline position for everyone else.

---

## What the code looks like

```cpp
// Each tick, client side:

// 1. Send input to server.
InputMessage msg{tick, localInput};
BitWriter bw;
serializeInput(bw, msg);
conn.send(ChannelId::ReliableUnordered, bw.finish());

// 2. Predict locally.
applyInputToLocalEntity(localInput, entity);           // update Transform
predBuf.push(tick, localInput, entity.transform);      // record for reconciliation

// 3. On snapshot arrival from server:
ReplicaSnapshot snap = decodeToReplica(payload, decodeWorld);
snapBuf.push(snap);                                    // for remote entity interpolation

// 4. Reconcile if server tick T disagrees with our prediction.
if (snap.tick > lastAckedTick) {
    predBuf.ackUpTo(snap.tick);
    if (serverTransform != predBuf.get(snap.tick)->predictedTransform) {
        entity.transform = serverTransform;            // snap
        for (auto& e : predBuf.getPending(snap.tick)) {
            applyInputToLocalEntity(e.input, entity);  // replay
        }
    }
}

// 5. Render: use prediction for local player, interpolation for remotes.
localRenderTransform = entity.transform;
remoteRenderTransform = snapBuf.interpolate(remoteId, renderTick).value_or(lastKnown);
```

---

## Things that surprised me

**Prediction only helps the local player.** Remote players are always delayed.
The "feels fast" experience belongs only to the entity you control.

**The server doesn't have to trust the client's prediction.** The client's
predicted state is purely local. The server runs its own simulation from the
inputs it receives. If the client sends "I moved right for 5 ticks" and the
server says "you were blocked after 2", the client corrects. The server is
the arbiter, not the average.

**Reconciliation is invisible at < 150 ms RTT.** At 150 ms, mispredictions
are ≈ 9 ticks. Replaying 9 ticks from a corrected state makes the local
entity jump by a few pixels — below the perception threshold at 60 Hz. At
> 300 ms it starts to be visible, which is why high-ping play feels jittery
even with prediction.

**The ring buffer size matters.** At 60 Hz with a 128-entry prediction buffer,
you have 2.1 s of history. An RTT of 400 ms = 24 ticks — easily within the
window. At 2 s RTT (pathological) entries would be evicted and the reconciliation
would silently lose precision. In practice, 400 ms is the upper limit for
playable multiplayer; the buffer is sized conservatively for that target.
