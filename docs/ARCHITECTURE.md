# Engine Architecture

> Technical documentation of the multiplayer game engine architecture

## Overview

This engine follows a **client-server architecture** where:
- The **server** is authoritative and runs game logic
- **Clients** handle input, prediction, and rendering

```
┌─────────────────────────────────────────────────────────────────┐
│                         GAME SERVER                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   Network   │  │    Game     │  │     State Manager       │  │
│  │   Manager   │◄─┤    World    │◄─┤  (Snapshots, Deltas)    │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
└────────────────────────────┬────────────────────────────────────┘
                             │ UDP
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│   CLIENT 1    │    │   CLIENT 2    │    │   CLIENT N    │
│ ┌───────────┐ │    │               │    │               │
│ │  Network  │ │    │     ...       │    │     ...       │
│ ├───────────┤ │    │               │    │               │
│ │Prediction │ │    │               │    │               │
│ ├───────────┤ │    │               │    │               │
│ │ Renderer  │ │    │               │    │               │
│ └───────────┘ │    │               │    │               │
└───────────────┘    └───────────────┘    └───────────────┘
```

## Core Systems

### Entity-Component-System (ECS)

We use an ECS architecture for flexibility and cache efficiency.

```cpp
// Entities are just IDs
using EntityId = uint32_t;

// Components are pure data
struct TransformComponent {
    float x, y;
    float rotation;
};

struct NetworkComponent {
    uint32_t networkId;
    bool isLocalPlayer;
};

struct RenderComponent {
    Color color;
    Shape shape;
};

// Systems operate on components
class MovementSystem {
    void update(World& world, float dt);
};
```

### Game Loop

Fixed timestep for deterministic simulation:

```
┌─────────────────────────────────────────────────────────┐
│                     GAME LOOP                            │
│                                                          │
│   accumulator += frameTime                               │
│                                                          │
│   while (accumulator >= TICK_RATE) {                    │
│       ┌─────────────────────────────────────────────┐   │
│       │  1. Process Input                            │   │
│       │  2. Send Input to Server (client)            │   │
│       │  3. Receive State Updates                    │   │
│       │  4. Fixed Update (physics, game logic)       │   │
│       │  5. Reconcile with Server (client)           │   │
│       └─────────────────────────────────────────────┘   │
│       accumulator -= TICK_RATE                          │
│   }                                                      │
│                                                          │
│   alpha = accumulator / TICK_RATE                       │
│   Render(interpolate(previousState, currentState, alpha))│
│                                                          │
└─────────────────────────────────────────────────────────┘
```

**Constants:**
- `TICK_RATE`: 1/60 seconds (60 Hz simulation)
- `NETWORK_RATE`: 1/20 seconds (20 Hz network updates)

## Network Architecture

### Protocol Stack

```
┌─────────────────────────────────────┐
│         Game Messages               │  PlayerInput, StateUpdate, etc.
├─────────────────────────────────────┤
│         Reliability Layer           │  ACKs, retransmission, ordering
├─────────────────────────────────────┤
│         Connection Layer            │  Handshake, keepalive, disconnect
├─────────────────────────────────────┤
│              UDP                    │  Low latency transport
└─────────────────────────────────────┘
```

### Message Types

| Type | Direction | Description |
|------|-----------|-------------|
| `CONNECT_REQUEST` | C→S | Client requests connection |
| `CONNECT_ACCEPT` | S→C | Server accepts, assigns player ID |
| `PLAYER_INPUT` | C→S | Input state (keys, mouse) |
| `STATE_SNAPSHOT` | S→C | Full world state |
| `STATE_DELTA` | S→C | Only changed entities |
| `DISCONNECT` | Both | Clean disconnection |

### Packet Format

```
┌────────────────────────────────────────────────────┐
│  Header (8 bytes)                                   │
│  ┌──────────┬──────────┬──────────┬──────────────┐ │
│  │ Sequence │   ACK    │ ACK Bits │    Type      │ │
│  │ (2 bytes)│ (2 bytes)│ (4 bytes)│   (1 byte)   │ │
│  └──────────┴──────────┴──────────┴──────────────┘ │
├────────────────────────────────────────────────────┤
│  Payload (variable)                                 │
│  ┌──────────────────────────────────────────────┐  │
│  │            Message-specific data              │  │
│  └──────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────┘
```

## Client-Side Prediction

To hide network latency, clients predict their own movement:

```
Client Input: ─────[1]────[2]────[3]────[4]────[5]────►
                    │      │      │      │      │
                    ▼      ▼      ▼      ▼      ▼
Local State:  ─────[P1]───[P2]───[P3]───[P4]───[P5]───►
                                         
Server ACK:   ────────────────────[S2]─────────────────►
                                   │
                                   ▼
                          Reconcile: Replay [3,4,5] on S2
```

### Reconciliation Algorithm

```cpp
void reconcileWithServer(const ServerState& serverState) {
    // Find the input that matches server's processed input
    while (!pendingInputs.empty() && 
           pendingInputs.front().sequence <= serverState.lastProcessedInput) {
        pendingInputs.pop_front();
    }
    
    // Start from server state
    localState = serverState;
    
    // Re-apply unacknowledged inputs
    for (const auto& input : pendingInputs) {
        applyInput(localState, input);
    }
}
```

## State Synchronization

### Snapshot Interpolation

Clients render between two received states for smoothness:

```
Server States:  ──[S1]────────[S2]────────[S3]────►
                              
Render Time:    ─────────[R]──────────────────────►
                         │
                         ▼
                   lerp(S1, S2, t)
```

**Interpolation delay:** ~100ms (configurable based on network conditions)

### Delta Compression

Only send what changed:

```cpp
struct StateDelta {
    uint32_t baselineSequence;  // What state this is relative to
    std::vector<EntityDelta> changes;
};

struct EntityDelta {
    EntityId id;
    uint8_t changedComponents;  // Bitmask
    // Only changed component data follows
};
```

## Threading Model

```
┌─────────────────────────────────────────────────────────┐
│                      MAIN THREAD                         │
│   Input → Game Logic → Prepare Render Data              │
└─────────────────────────────────────────────────────────┘
                          │
         ┌────────────────┼────────────────┐
         ▼                ▼                ▼
┌─────────────────┐ ┌───────────┐ ┌─────────────────┐
│  RENDER THREAD  │ │  NETWORK  │ │  AUDIO THREAD   │
│                 │ │  THREAD   │ │   (future)      │
│  OpenGL calls   │ │  Send/Recv│ │                 │
└─────────────────┘ └───────────┘ └─────────────────┘
```

## Memory Layout

For cache efficiency, components are stored contiguously:

```
TransformComponents:  [T0][T1][T2][T3][T4][T5]...
VelocityComponents:   [V0][V1][V2][V3][V4][V5]...
RenderComponents:     [R0][R1][R2][R3][R4][R5]...
```

Systems iterate over component arrays linearly for optimal cache utilization.

## Performance Targets

| Metric | Target |
|--------|--------|
| Server tick rate | 60 Hz |
| Network update rate | 20 Hz |
| Max players per server | 16+ |
| State snapshot size | < 1 KB |
| Client-to-server latency tolerance | Up to 200ms |
| Memory per entity | < 256 bytes |

## Future Considerations

- [ ] Interest management (only send relevant entities)
- [ ] Spatial partitioning for large worlds
- [ ] Replay system for debugging
- [ ] Anti-cheat considerations
- [ ] Voice chat integration
