# Sumo Arena — Engine Testbed

> **Purpose:** a deliberately minimal PvP game used to stress-test every hard
> problem in the networking engine. The engine is the product; this is the
> exercise that forces us to build it right.
>
> The [Dungeon Crawler](GAME_DESIGN.md) remains on the roadmap as a later
> showcase built on the same engine.

---

## 🎯 Why this testbed

Sumo Arena was chosen because it is the **simplest shape of game that still
forces every hard netcode problem** to show up:

| Engine feature | How Sumo forces it |
|---|---|
| Deterministic fixed-tick sim     | Physics knockback must resolve identically on all peers. |
| Snapshot + delta serialization   | 4–16 players × moving bodies × many updates per second. |
| Client-side prediction           | Dash and shove feel terrible without it. |
| Server reconciliation            | Mispredictions are visible because shoves matter. |
| Lag compensation                 | Contact hits must be fair across 50–150 ms RTT. |
| Interest management              | Spectators and eliminated players don't need full updates. |
| Rollback-friendliness *(opt.)*   | No hidden state → clean rollback once snapshots land. |

Things Sumo deliberately skips: gunplay, inventories, dialogue, level design,
art. Every cycle spent there is a cycle not spent on the engine.

---

## 🎮 Core concept

**Elevator pitch:** 4–16 players as bouncy blobs on a shrinking platform.
Dash into each other, shove opponents off the edge, last one standing wins.

- **Genre:** top-down physics PvP / party brawler
- **Perspective:** 2D top-down
- **Round length:** 60–90 seconds
- **Session:** best-of-5 rounds, matchmaking or direct connect
- **Players:** MVP = 4; stretch target = 16

---

## 🕹️ Core loop

```
┌──────────────┐   ┌─────────────┐   ┌──────────────┐   ┌──────────────┐
│ CONNECT /    │──►│  ROUND      │──►│  SCOREBOARD  │──►│ NEXT ROUND / │
│ LOBBY        │   │  (60-90s)   │   │              │   │ END MATCH    │
└──────────────┘   └──────┬──────┘   └──────────────┘   └──────────────┘
                          │
                          ▼
              ┌──────────────────────────┐
              │  MOVE → DASH → SHOVE     │
              │         ↑          │     │
              │         └──────────┘     │
              │  arena shrinks over time │
              └──────────────────────────┘
```

Win: be the last player on the platform. Loss: fall off the edge.

---

## ⚔️ Mechanics (MVP)

| Action | Input | Cooldown | Effect |
|---|---|---|---|
| Move    | WASD / left stick      | — | Apply acceleration to velocity (not teleport). |
| Dash    | Space / A              | 1.0 s | Short burst of velocity in facing direction; 150 ms i-frames. |
| Shove   | LMB / RT               | 0.5 s | Short-range AoE impulse on overlapping bodies. |
| *(later)* Grab | RMB / LT         | 2.0 s | Latch to an opponent; both players locked for 0.5 s. |

Everything is momentum-based: there is no HP, only position and velocity.
You "die" when your position leaves the arena polygon.

---

## 🧱 ECS surface

New components needed beyond what exists today:

```cpp
namespace game {

struct RigidBody {            // replaces bare Velocity for players
    float vx, vy;
    float mass;
    float drag;               // linear damping per second
};

struct Collider {             // circle for MVP
    float radius;
    uint16_t layer;           // bitmask: Player, Arena, Projectile
    uint16_t mask;
};

struct DashAbility {
    float cooldownRemaining;
    float iframesRemaining;
    float dashSpeed;
};

struct ShoveAbility {
    float cooldownRemaining;
    float range;
    float impulse;
};

struct PlayerNet {            // ties an entity to a networked player
    uint32_t netId;
    uint16_t lastInputSeq;
};

struct ArenaBounds {           // singleton entity
    float centerX, centerY;
    float radius;              // shrinks over round
    float shrinkRate;
};

} // namespace game
```

New systems:

| System | Runs on | Reads | Writes |
|---|---|---|---|
| `CollisionSystem`   | sim | `Transform`, `Collider`, `RigidBody` | `RigidBody`, contact events |
| `PhysicsSystem`     | sim | `RigidBody`, `Transform`             | `Transform` |
| `DashSystem`        | sim | `PlayerInput`, `DashAbility`, `RigidBody` | `RigidBody`, `DashAbility` |
| `ShoveSystem`       | sim | `PlayerInput`, `ShoveAbility`, `Transform`, `Collider` | `RigidBody` of neighbours |
| `ArenaSystem`       | sim | `ArenaBounds`, `Transform` | destroys entities, spawns eliminations |
| `EliminationSystem` | server-only | eliminations channel | round state |

The `RenderSystem` stays a client-only concern (after the `Engine` → `Simulation`
split, which lands before Phase 3).

---

## 🛰️ Network design notes

- **Tick:** 60 Hz sim, 20 Hz snapshot send rate (server → client).
- **Client input rate:** 60 Hz, coalesced — input includes sequence number.
- **Authority:** server simulates everything; clients only predict their own
  player. Other players are snapshot-interpolated ~100 ms in the past.
- **Delta compression:** per-entity dirty bits from a per-client baseline ack.
- **Reliability channels:** unreliable (snapshots), reliable-unordered (events
  like eliminations), reliable-ordered (round start/end, lobby).
- **Lag compensation:** shove hit-check rewinds victim positions to the
  attacker's view-time (tick = `currentTick − clientRTT/2`).
- **Bandwidth target:** < 32 KB/s down per client with 16 players active.

---

## 📏 MVP scope (what ships at the end of Phase 5)

Must-have:
- [ ] One circular arena, fixed size (shrink comes later).
- [ ] Move + dash + shove.
- [ ] 4 players on a local server from separate clients over LAN.
- [ ] Server-authoritative elimination detection.
- [ ] Client-side prediction + server reconciliation for the local player.
- [ ] Snapshot interpolation for remote players.
- [ ] Lag comp for shove hits (behind a flag so you can A/B it in demos).
- [ ] HUD: round timer, player count, your-turn-to-be-eliminated indicator.

Nice-to-have, post-MVP:
- [ ] Shrinking arena, power-ups, grab, multiple arena shapes.
- [ ] Match flow: lobby → best-of-5 → scoreboard.
- [ ] Spectator view (tests AoI + read-only replication).
- [ ] Replay: save and replay matches from input log (leverages Phase 2).

Explicit non-goals (for now):
- Animations beyond color + scale pulses.
- Audio.
- Menus beyond a direct-connect IP box.
- Cheat prevention (server authority is enough for the testbed).

---

## 🔬 What this testbed must let us measure

Benchmarks that must exist before we call the engine "done":

- Snapshot size distribution (full + delta) for 4 / 8 / 16 players.
- Server tick time histogram under load.
- End-to-end input latency at 0 / 50 / 100 / 150 ms simulated RTT.
- Perceived smoothness at 0 / 2 / 5 / 10 % simulated packet loss.
- CPU + RSS of a 16-player server on a $5/mo VPS-class machine.

These numbers get committed to `bench/results.md` so we can watch them
improve over time. See [ROADMAP.md](ROADMAP.md) for the engine targets.

---

## 🧭 Relationship to the dungeon crawler

The existing [GAME_DESIGN.md](GAME_DESIGN.md) dungeon crawler is **not
cancelled** — it's parked as the post-engine showcase. Once the engine hits
its performance targets against Sumo Arena, the dungeon crawler becomes a
second testbed proving the engine generalises beyond PvP physics.

Both games share the same engine, ECS, netcode, and serialization layer.
Nothing in `include/engine/` should ever mention "sumo" or "dungeon."
