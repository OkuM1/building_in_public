#pragma once
#include "World.h"

/**
 * @file System.h
 * @brief Base interface for ECS systems.
 *
 * A system is a pure function of the @ref World : given the current state,
 * it mutates components to produce the next state. Systems **own no data**
 * beyond construction parameters; all persistent state lives in components
 * so it can be snapshotted, replayed, and serialised for the network.
 *
 * Execution order is determined by the order systems are added to the owner
 * (currently `engine::Simulation` after Phase 2.5, previously `Engine`).
 *
 * @see docs/design/0001-simulation-split.md
 * @see docs/CODE_DOCS.md  Header-comment style we follow.
 */

namespace engine {

/// @brief Abstract base for anything that runs once per fixed simulation tick.
///
/// Concrete systems override @ref update and should be added to the
/// simulation's system list. Keep the interface intentionally tiny: any
/// system-specific configuration belongs in the concrete type's constructor,
/// not on this base.
///
/// @note Rendering is deliberately **not** a `System` \u2014 it is a free
///       function (`client::renderWorld`) driven by the client at render
///       rate rather than sim rate. See design note 0001.
class System {
public:
    virtual ~System() = default;

    /// @brief Advance this system by one simulation tick.
    ///
    /// @param world  Mutable access to the ECS world. Systems may read and
    ///               write any components they were built to operate on.
    /// @param dt     Fixed simulation timestep in seconds. Constant across
    ///               a run (currently 1/60). Passed here rather than pulled
    ///               from a global so tests can step the simulation
    ///               deterministically.
    ///
    /// @pre Every component type the system reads or writes has been
    ///      registered on @p world.
    virtual void update(World& world, float dt) = 0;
};

} // namespace engine
