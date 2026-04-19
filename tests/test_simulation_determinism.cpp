#include "doctest.h"

#include "engine/core/Simulation.h"
#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
#include "engine/ecs/World.h"
#include "game/components/GameComponents.h"

#include <cstdint>
#include <memory>
#include <random>

/**
 * @file test_simulation_determinism.cpp
 * @brief Proves two independently constructed `engine::Simulation`
 *        instances, fed the same inputs, produce identical `Transform`s.
 * @see docs/design/0007-determinism-test.md
 */

namespace {

/// @brief Builds a sim with the standard Phase 2.5 system order and
///        returns the player entity id.
engine::EntityId buildSim(engine::Simulation& sim) {
    auto& world = sim.world();
    world.registerComponent<game::Transform>();
    world.registerComponent<game::PreviousTransform>();
    world.registerComponent<game::Velocity>();
    world.registerComponent<game::PlayerInput>();

    sim.addSystem(std::make_unique<engine::InputSystem>());
    sim.addSystem(std::make_unique<engine::MovementSystem>());

    auto player = world.createEntity();
    world.addComponent(player, game::Transform{0.0f, 0.0f, 0.0f});
    world.addComponent(player, game::PreviousTransform{0.0f, 0.0f, 0.0f});
    world.addComponent(player, game::Velocity{0.0f, 0.0f});
    world.addComponent(player, game::PlayerInput{});
    return player;
}

/// @brief Overwrite the PlayerInput component in-place.
void setInput(engine::World& world, engine::EntityId e,
              bool up, bool down, bool left, bool right) {
    auto& in = world.getComponent<game::PlayerInput>(e);
    in.moveUp    = up;
    in.moveDown  = down;
    in.moveLeft  = left;
    in.moveRight = right;
}

} // namespace

TEST_CASE("Simulation is deterministic across two instances, canned schedule") {
    engine::Simulation a;
    engine::Simulation b;
    const auto pa = buildSim(a);
    const auto pb = buildSim(b);

    // Schedule: 60 ticks right, 60 ticks up+right (diagonal), 60 ticks
    // no input (coast at zero velocity because there's no friction), 60
    // ticks left.
    struct Phase { int ticks; bool u, d, l, r; };
    const Phase schedule[] = {
        {60, false, false, false, true},
        {60, true,  false, false, true},
        {60, false, false, false, false},
        {60, false, false, true,  false},
    };

    for (const auto& phase : schedule) {
        for (int i = 0; i < phase.ticks; ++i) {
            setInput(a.world(), pa, phase.u, phase.d, phase.l, phase.r);
            setInput(b.world(), pb, phase.u, phase.d, phase.l, phase.r);
            a.step();
            b.step();
        }
    }

    const auto& ta = a.world().getComponent<game::Transform>(pa);
    const auto& tb = b.world().getComponent<game::Transform>(pb);

    CHECK(ta.x == tb.x);
    CHECK(ta.y == tb.y);
    CHECK(ta.rotation == tb.rotation);
    CHECK(a.tick() == b.tick());
    CHECK(a.tick() == 240u);
}

TEST_CASE("Simulation reaches a closed-form position under constant right input") {
    engine::Simulation sim;
    const auto player = buildSim(sim);

    setInput(sim.world(), player, false, false, false, true);
    for (int i = 0; i < 60; ++i) sim.step();

    const auto& t = sim.world().getComponent<game::Transform>(player);

    // 60 ticks * FIXED_DT * kPlayerSpeed = 1s * 0.5 u/s = 0.5 units.
    const float expected = 60 * engine::Simulation::FIXED_DT * 0.5f;
    CHECK(t.x == doctest::Approx(expected).epsilon(1e-5));
    CHECK(t.y == doctest::Approx(0.0f).epsilon(1e-5));
}

TEST_CASE("Simulation is deterministic under seeded random input schedule") {
    // Two sims, same seed, same draws -> same draws of booleans.
    auto runRandom = [](std::uint32_t seed, int ticks) {
        engine::Simulation sim;
        const auto player = buildSim(sim);
        std::mt19937 rng(seed);
        std::bernoulli_distribution coin(0.5);

        for (int i = 0; i < ticks; ++i) {
            setInput(sim.world(), player,
                     coin(rng), coin(rng), coin(rng), coin(rng));
            sim.step();
        }
        return sim.world().getComponent<game::Transform>(player);
    };

    const auto ta = runRandom(12345u, 600);
    const auto tb = runRandom(12345u, 600);

    CHECK(ta.x == tb.x);
    CHECK(ta.y == tb.y);
    CHECK(ta.rotation == tb.rotation);
}
