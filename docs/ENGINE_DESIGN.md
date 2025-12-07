# Engine Design - Clean Architecture

> Designing the engine properly before implementation

## 🎯 Design Principles

1. **Separation of Concerns** - Each system does one thing well
2. **Data-Oriented** - Components are data, systems are logic
3. **Minimal Dependencies** - Systems don't know about each other
4. **Testable** - Can test systems in isolation
5. **Network-Ready** - State is serializable from day one

---

## 📐 Layer Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                            GAME                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  DungeonCrawler                                              ││
│  │  - GameSession (lobby, floor progression)                   ││
│  │  - DungeonGenerator                                          ││
│  │  - LootTables                                                ││
│  └─────────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────────┤
│                          ENGINE                                  │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐ │
│  │     ECS      │ │   Systems    │ │       Services           │ │
│  │              │ │              │ │                          │ │
│  │ - World      │ │ - Movement   │ │ - Input                  │ │
│  │ - Entity     │ │ - Combat     │ │ - Renderer               │ │
│  │ - Component  │ │ - AI         │ │ - Network (future)       │ │
│  │ - Registry   │ │ - Collision  │ │ - Audio (future)         │ │
│  └──────────────┘ └──────────────┘ └──────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                         PLATFORM                                 │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────────┐ │
│  │    Window    │ │   Graphics   │ │       Time               │ │
│  │    (GLFW)    │ │   (OpenGL)   │ │    (chrono)              │ │
│  └──────────────┘ └──────────────┘ └──────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Layer Rules

| Layer | Can Depend On | Cannot Depend On |
|-------|---------------|------------------|
| Game | Engine, Platform | - |
| Engine | Platform | Game |
| Platform | Nothing | Engine, Game |

---

## 🧩 ECS Design

### What is ECS?

```
ENTITY          COMPONENTS              SYSTEMS
  │                 │                      │
  ▼                 ▼                      ▼
Just an ID    Pure data structs      Logic that operates
(uint32_t)    (no methods)           on components

Entity 1 ─────► [Transform] [Health] [PlayerInput]
                     │          │           │
Entity 2 ─────► [Transform] [Health] [AIBrain]
                     │          │           │
                     ▼          ▼           ▼
              MovementSystem  CombatSystem  AISystem
              (reads Transform, (reads Health) (reads AIBrain,
               writes position)              writes Transform)
```

### Why ECS?

| Problem | Traditional OOP | ECS Solution |
|---------|-----------------|--------------|
| "Player is a Character is a Entity is a..." | Deep inheritance | Composition |
| "Where does combat code go?" | Fat base classes | Dedicated system |
| Cache misses | Objects scattered in memory | Components packed |
| Networking | Serialize entire objects | Serialize components |
| Testing | Need full object hierarchy | Test system with mock data |

---

## 📦 Components (Data Only)

```cpp
// ═══════════════════════════════════════════════════════════════
// CORE COMPONENTS - Used by engine systems
// ═══════════════════════════════════════════════════════════════

struct Transform {
    float x, y;              // World position
    float rotation;          // Facing direction (radians)
};

struct Velocity {
    float vx, vy;            // Movement per second
};

struct Collider {
    float width, height;     // Bounding box
    bool isTrigger;          // Pass through but detect?
};

// ═══════════════════════════════════════════════════════════════
// GAMEPLAY COMPONENTS - Game-specific
// ═══════════════════════════════════════════════════════════════

struct Health {
    int current;
    int max;
    bool isDead() const { return current <= 0; }
};

struct Combat {
    int attackDamage;
    float attackCooldown;    // Seconds between attacks
    float cooldownTimer;     // Current cooldown remaining
};

struct PlayerInput {
    bool moveUp, moveDown, moveLeft, moveRight;
    bool attack;
    bool dodge;
};

struct AIBrain {
    enum class State { Idle, Chasing, Attacking, Fleeing };
    State state = State::Idle;
    EntityId target = INVALID_ENTITY;
    float thinkTimer = 0.0f;
};

struct Renderable {
    enum class Shape { Rectangle, Circle };
    Shape shape = Shape::Rectangle;
    float r, g, b;           // Color
    int layer;               // Draw order (higher = on top)
};

// ═══════════════════════════════════════════════════════════════
// TAG COMPONENTS - No data, just mark entities
// ═══════════════════════════════════════════════════════════════

struct Player {};            // Entity is a player
struct Enemy {};             // Entity is an enemy
struct Projectile {};        // Entity is a projectile
struct Item {};              // Entity is a pickup item
```

---

## ⚙️ Systems (Logic Only)

Each system operates on specific component combinations:

```cpp
// ═══════════════════════════════════════════════════════════════
// SYSTEM INTERFACE
// ═══════════════════════════════════════════════════════════════

class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, float dt) = 0;
};

// ═══════════════════════════════════════════════════════════════
// MOVEMENT SYSTEM
// Operates on: Transform, Velocity
// ═══════════════════════════════════════════════════════════════

class MovementSystem : public System {
public:
    void update(World& world, float dt) override {
        // For each entity with Transform AND Velocity:
        //   transform.x += velocity.vx * dt
        //   transform.y += velocity.vy * dt
    }
};

// ═══════════════════════════════════════════════════════════════
// INPUT SYSTEM
// Operates on: PlayerInput, Velocity (for players)
// ═══════════════════════════════════════════════════════════════

class InputSystem : public System {
public:
    void update(World& world, float dt) override {
        // For each entity with PlayerInput AND Velocity:
        //   Read keyboard state
        //   Set velocity based on input
    }
};

// ═══════════════════════════════════════════════════════════════
// AI SYSTEM
// Operates on: AIBrain, Transform, Velocity (for enemies)
// ═══════════════════════════════════════════════════════════════

class AISystem : public System {
public:
    void update(World& world, float dt) override {
        // For each entity with AIBrain:
        //   Update state machine
        //   Set velocity toward target
    }
};

// ═══════════════════════════════════════════════════════════════
// COLLISION SYSTEM
// Operates on: Transform, Collider
// ═══════════════════════════════════════════════════════════════

class CollisionSystem : public System {
public:
    void update(World& world, float dt) override {
        // Check all collider pairs
        // Generate collision events
    }
};

// ═══════════════════════════════════════════════════════════════
// COMBAT SYSTEM
// Operates on: Health, Combat
// ═══════════════════════════════════════════════════════════════

class CombatSystem : public System {
public:
    void update(World& world, float dt) override {
        // Process attack commands
        // Apply damage
        // Update cooldowns
    }
};

// ═══════════════════════════════════════════════════════════════
// RENDER SYSTEM
// Operates on: Transform, Renderable
// ═══════════════════════════════════════════════════════════════

class RenderSystem : public System {
public:
    void update(World& world, float dt) override {
        // Sort by layer
        // Draw each entity
    }
};
```

### System Execution Order

Order matters! Dependencies flow downward:

```
┌─────────────────┐
│   InputSystem   │  1. Read player input
└────────┬────────┘
         ▼
┌─────────────────┐
│    AISystem     │  2. Enemies decide what to do
└────────┬────────┘
         ▼
┌─────────────────┐
│ MovementSystem  │  3. Apply velocities to positions
└────────┬────────┘
         ▼
┌─────────────────┐
│CollisionSystem  │  4. Detect overlaps
└────────┬────────┘
         ▼
┌─────────────────┐
│  CombatSystem   │  5. Process hits and damage
└────────┬────────┘
         ▼
┌─────────────────┐
│  RenderSystem   │  6. Draw everything
└─────────────────┘
```

---

## 🗂️ Folder Structure

```
include/
├── engine/
│   ├── ecs/
│   │   ├── Entity.h           # EntityId type, constants
│   │   ├── Component.h        # Component base, type IDs
│   │   ├── World.h            # Entity + component storage
│   │   └── System.h           # System interface
│   ├── systems/
│   │   ├── MovementSystem.h
│   │   ├── CollisionSystem.h
│   │   ├── RenderSystem.h
│   │   └── InputSystem.h
│   ├── core/
│   │   ├── Engine.h           # Main game loop
│   │   ├── Time.h             # Delta time, fixed timestep
│   │   └── Logger.h           # Logging utility
│   └── platform/
│       ├── Window.h           # GLFW wrapper
│       └── Input.h            # Keyboard/mouse state
│
├── game/
│   ├── components/
│   │   ├── GameComponents.h   # Health, Combat, AIBrain, etc.
│   │   └── Tags.h             # Player, Enemy, Item tags
│   ├── systems/
│   │   ├── AISystem.h
│   │   └── CombatSystem.h
│   └── DungeonGame.h          # Game setup, level loading

src/
├── engine/
│   ├── ecs/
│   │   └── World.cpp
│   ├── systems/
│   │   ├── MovementSystem.cpp
│   │   ├── CollisionSystem.cpp
│   │   └── RenderSystem.cpp
│   ├── core/
│   │   └── Engine.cpp
│   └── platform/
│       └── Window.cpp
│
├── game/
│   ├── systems/
│   │   ├── AISystem.cpp
│   │   └── CombatSystem.cpp
│   └── DungeonGame.cpp
│
├── client/
│   └── main.cpp               # Client entry point
│
└── server/
    └── main.cpp               # Server entry point (headless)
```

---

## 🔌 Interfaces

### World Interface

```cpp
class World {
public:
    // Entity management
    EntityId createEntity();
    void destroyEntity(EntityId id);
    bool isAlive(EntityId id) const;
    
    // Component management
    template<typename T>
    T& addComponent(EntityId id);
    
    template<typename T>
    T& getComponent(EntityId id);
    
    template<typename T>
    bool hasComponent(EntityId id) const;
    
    template<typename T>
    void removeComponent(EntityId id);
    
    // Iteration
    template<typename... Components>
    View<Components...> view();
    // Usage: for (auto [entity, transform, health] : world.view<Transform, Health>())
};
```

### Engine Interface

```cpp
class Engine {
public:
    Engine(int width, int height, const std::string& title);
    ~Engine();
    
    void run();                          // Main loop
    void stop();                         // Request shutdown
    
    World& getWorld();                   // Access ECS world
    
    void addSystem(std::unique_ptr<System> system);
    
    // Services
    Input& getInput();
    // Renderer& getRenderer();          // Future
    // NetworkManager& getNetwork();     // Future
    
private:
    void fixedUpdate(float dt);          // 60 Hz game logic
    void render(float alpha);            // Variable rate rendering
};
```

---

## 🎮 Example: Creating a Player

```cpp
void DungeonGame::spawnPlayer(World& world, float x, float y) {
    EntityId player = world.createEntity();
    
    // Core components
    world.addComponent<Transform>(player) = { x, y, 0.0f };
    world.addComponent<Velocity>(player) = { 0.0f, 0.0f };
    world.addComponent<Collider>(player) = { 0.8f, 0.8f, false };
    
    // Gameplay components
    world.addComponent<Health>(player) = { 100, 100 };
    world.addComponent<Combat>(player) = { 10, 0.5f, 0.0f };
    world.addComponent<PlayerInput>(player) = {};
    
    // Rendering
    world.addComponent<Renderable>(player) = { 
        Renderable::Shape::Rectangle,
        0.2f, 0.8f, 0.2f,  // Green
        10                  // Layer
    };
    
    // Tags
    world.addComponent<Player>(player);
}
```

---

## 📡 Network-Ready Design

Components are already serialization-friendly:

```cpp
// Transform can be serialized as:
// [float x][float y][float rotation] = 12 bytes

// Full player state:
// [EntityId][Transform][Health][Combat state] ≈ 32 bytes

// Snapshot of 4 players + 20 enemies:
// 4 * 32 + 20 * 24 ≈ 600 bytes per tick
```

### Network Component (Future)

```cpp
struct NetworkIdentity {
    uint32_t networkId;      // Unique across network
    bool isOwner;            // Does this client control it?
    bool isDirty;            // Has state changed?
};
```

---

## ✅ Design Checklist

Before implementing, verify:

- [ ] Each component is pure data (no logic)
- [ ] Each system has clear input/output components
- [ ] No system depends on another system directly
- [ ] Layer boundaries are respected
- [ ] All game state is in components (serializable)
- [ ] No global mutable state

---

## 🚀 Implementation Order

1. **Entity + World** (minimal ECS)
2. **Transform + Renderable + RenderSystem** (see things)
3. **PlayerInput + InputSystem** (move things)
4. **Velocity + MovementSystem** (physics-ready movement)
5. **Collider + CollisionSystem** (boundaries)
6. **AIBrain + AISystem** (enemies)
7. **Health + Combat + CombatSystem** (damage)

Each step is testable and visible!
