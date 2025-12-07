# Dungeon Crawler - Game Design Document

> A cooperative multiplayer dungeon crawler for 1-4 players

## 🎮 Core Concept

**Elevator Pitch:** Team up with friends to explore procedurally generated dungeons, fight monsters, collect loot, and survive as deep as you can.

**Genre:** Top-down co-op action RPG / Roguelite  
**Players:** 1-4 (online multiplayer)  
**Perspective:** Top-down 2D  
**Session Length:** 15-30 minutes per run

---

## 🎯 Design Pillars

1. **Cooperation** - Players must work together; lone wolves struggle
2. **Accessible** - Easy to pick up, satisfying immediately
3. **Replayable** - Procedural generation + unlocks keep it fresh
4. **Networked** - Smooth multiplayer is a first-class feature

---

## 🕹️ Core Gameplay Loop

```
┌─────────────────────────────────────────────────────────────┐
│                        GAME SESSION                          │
│                                                              │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐  │
│   │  LOBBY  │───►│  FLOOR  │───►│  FLOOR  │───►│  BOSS   │  │
│   │         │    │    1    │    │   2-N   │    │  FLOOR  │  │
│   └─────────┘    └─────────┘    └─────────┘    └─────────┘  │
│                       │              │              │        │
│                       ▼              ▼              ▼        │
│                  ┌─────────────────────────────────────┐    │
│                  │     EXPLORE → FIGHT → LOOT         │    │
│                  │            ↑         │             │    │
│                  │            └─────────┘             │    │
│                  └─────────────────────────────────────┘    │
│                                                              │
│   Win: Defeat boss  │  Lose: All players dead               │
└─────────────────────────────────────────────────────────────┘
```

---

## 🏰 Dungeon Structure

### Floor Layout
- Grid-based rooms connected by corridors
- Each floor: 5-10 rooms
- Room types:
  - **Combat rooms** - Enemies spawn, doors lock until cleared
  - **Treasure rooms** - Loot chests, sometimes trapped
  - **Shop rooms** - Spend gold on items
  - **Rest rooms** - Heal, but enemies may ambush
  - **Boss room** - Floor exit, guarded by boss

### Procedural Generation (MVP)
```
1. Place starting room at center
2. Use BSP or random walk to create room layout
3. Ensure all rooms are connected
4. Place exit room furthest from start
5. Populate rooms with enemies/loot based on floor depth
```

---

## ⚔️ Combat System

### Player Actions
| Action | Input | Cooldown | Notes |
|--------|-------|----------|-------|
| Move | Arrow keys / WASD | - | Grid-based or free movement |
| Attack | Space / LMB | 0.5s | Melee swing in facing direction |
| Dodge | Shift | 1.5s | Brief invincibility + dash |
| Use Item | 1-4 keys | Varies | Consumables (potions, bombs) |
| Special | Q | 5-10s | Class-specific ability |

### Combat Feel
- **Responsive** - Actions happen immediately (client prediction)
- **Impactful** - Screen shake, flash on hit
- **Readable** - Clear enemy telegraphs before attacks

---

## 👤 Player Classes (MVP: 2 classes)

### Warrior
- High HP, melee focused
- **Attack:** Sword swing (arc in front)
- **Special:** Shield bash (stun enemies)
- Role: Tank, front-line fighter

### Ranger  
- Medium HP, ranged attacks
- **Attack:** Arrow shot (projectile)
- **Special:** Multi-shot (3 arrows in spread)
- Role: Damage dealer, kiting

### Future Classes
- Mage (AoE damage, crowd control)
- Healer (support, buffs)
- Rogue (high damage, stealth)

---

## 👾 Enemies

### Basic Enemies
| Enemy | Behavior | Danger |
|-------|----------|--------|
| Slime | Hops toward nearest player | Low |
| Skeleton | Walks and swings sword | Medium |
| Archer | Keeps distance, shoots arrows | Medium |
| Bat | Fast, erratic movement | Low |

### Elite Enemies (spawn after floor 3)
| Enemy | Behavior | Danger |
|-------|----------|--------|
| Armored Knight | Blocks frontal attacks | High |
| Necromancer | Summons skeletons | High |
| Mimic | Disguised as chest, ambush | Medium |

### Boss (End of each floor set)
- Large health pool
- Multiple attack patterns
- Phases that change behavior
- Example: **Skeleton King** - summons minions, ground slam attack

---

## 🎒 Items & Loot

### Item Types
| Type | Examples | Rarity |
|------|----------|--------|
| **Consumables** | Health potion, Bomb, Speed boost | Common |
| **Weapons** | Better sword, Magic bow | Uncommon-Rare |
| **Armor** | Leather cap, Iron boots | Uncommon-Rare |
| **Accessories** | Ring of haste, Amulet of life | Rare |

### Loot Distribution
- Killed enemies drop gold + chance for items
- Chests guarantee items
- Rarity increases with floor depth
- **Shared loot** - items visible to all, first pickup gets it (encourages communication)

---

## 🌐 Multiplayer Architecture

### Network Model
```
                    ┌─────────────────────┐
                    │   AUTHORITATIVE     │
                    │      SERVER         │
                    │                     │
                    │  - Game state       │
                    │  - Enemy AI         │
                    │  - Hit detection    │
                    │  - Loot spawning    │
                    └──────────┬──────────┘
                               │
          ┌────────────────────┼────────────────────┐
          │                    │                    │
          ▼                    ▼                    ▼
    ┌───────────┐        ┌───────────┐        ┌───────────┐
    │  Client 1 │        │  Client 2 │        │  Client 3 │
    │  (Host?)  │        │           │        │           │
    │           │        │           │        │           │
    │ - Input   │        │ - Input   │        │ - Input   │
    │ - Render  │        │ - Render  │        │ - Render  │
    │ - Predict │        │ - Predict │        │ - Predict │
    └───────────┘        └───────────┘        └───────────┘
```

### What the Server Handles
- Enemy spawning and AI decisions
- Damage calculation and HP
- Loot drops and item pickups
- Room clearing state
- Player death and respawn

### What Clients Predict
- Local player movement
- Attack animations
- Projectile trajectories

### Sync Priorities
| Data | Update Rate | Reliability |
|------|-------------|-------------|
| Player position | 20 Hz | Unreliable (latest wins) |
| Player HP | On change | Reliable |
| Enemy position | 10 Hz | Unreliable |
| Loot pickup | On event | Reliable |
| Room state | On change | Reliable |

---

## 📊 Technical Specifications

### Game Constants
```cpp
constexpr int TICK_RATE = 60;           // Server simulation rate
constexpr int NETWORK_RATE = 20;        // Network updates per second
constexpr int MAX_PLAYERS = 4;
constexpr int MAX_ENEMIES_PER_ROOM = 12;
constexpr int GRID_SIZE = 32;           // Pixels per grid cell (if grid-based)
```

### Entity Budget
| Entity Type | Max Count | State Size |
|-------------|-----------|------------|
| Players | 4 | ~64 bytes each |
| Enemies | 50 | ~32 bytes each |
| Projectiles | 100 | ~16 bytes each |
| Items | 50 | ~16 bytes each |
| **Total** | ~204 | ~3 KB max |

---

## 🚀 Development Milestones

### MVP (Minimum Viable Product)
- [ ] Single room with 1 player
- [ ] Basic enemy (slime) with AI
- [ ] Melee combat
- [ ] Player health and death
- [ ] Local multiplayer (2 players, same machine - for testing)

### Alpha (Networked)
- [ ] Client-server architecture working
- [ ] 2 players online in same room
- [ ] Combat synced correctly
- [ ] Basic lobby system

### Beta (Content)
- [ ] Multiple rooms and floor generation
- [ ] 2 player classes
- [ ] 4 enemy types
- [ ] Loot system
- [ ] Boss fight

### Release
- [ ] 4 player support
- [ ] Cloud-hosted servers
- [ ] Polish (audio, particles, juice)
- [ ] Progression system

---

## 🎨 Art Style (Placeholder)

For development, use simple shapes:
- **Players:** Colored rectangles (you already have this!)
- **Enemies:** Different colored rectangles
- **Walls:** Gray rectangles
- **Items:** Small colored squares

Later: Pixel art or simple vector graphics

---

## 📚 References & Inspiration

- **Enter the Gungeon** - Room-based combat, co-op
- **Nuclear Throne** - Fast action, procedural
- **Gauntlet** - Classic co-op dungeon crawler
- **Realm of the Mad God** - Browser MMO dungeon crawler
- **Diablo** - Loot systems, satisfying combat

---

## ❓ Open Questions

1. **Grid-based or free movement?** 
   - Grid: Easier networking, fits current code
   - Free: More fluid combat feel
   
2. **Persistent progression?**
   - Roguelite (meta-progression between runs)
   - Pure roguelike (fresh start each time)

3. **Hosting model?**
   - Dedicated servers (best experience, costs money)
   - Player-hosted (one player is server, free but NAT issues)
   - Hybrid (matchmaking server + player hosts)

---

*This is a living document. Update as the game evolves!*
