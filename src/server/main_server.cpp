#include "engine/core/Simulation.h"
#include "engine/systems/InputSystem.h"
#include "engine/systems/MovementSystem.h"
#include "engine/core/Logger.h"
#include "game/components/GameComponents.h"

#include <chrono>
#include <thread>
#include <memory>

/**
 * @file main_server.cpp
 * @brief Headless server stub. Ticks a simulation at 60 Hz and logs.
 *
 * This executable exists to prove the build graph works without GL: it
 * links @c simulation only. No keyboard, no window, no renderer.
 *
 * @see docs/design/0006-cmake-split.md
 */

namespace {

void RegisterComponents(engine::World& world) {
    world.registerComponent<game::Transform>();
    world.registerComponent<game::PreviousTransform>();
    world.registerComponent<game::Renderable>();
    world.registerComponent<game::Velocity>();
    world.registerComponent<game::PlayerInput>();
    world.registerComponent<game::Player>();
    world.registerComponent<game::Enemy>();
}

} // namespace

int main() {
    using clock = std::chrono::steady_clock;
    using secondsf = std::chrono::duration<float>;

    engine::Logger::Info("Starting headless server...");

    engine::Simulation sim;
    RegisterComponents(sim.world());
    sim.addSystem(std::make_unique<engine::InputSystem>());
    sim.addSystem(std::make_unique<engine::MovementSystem>());

    // Spawn a single no-input entity so something exists to tick over.
    auto& world = sim.world();
    auto id = world.createEntity();
    world.addComponent(id, game::Transform{0.0f, 0.0f, 0.0f});
    world.addComponent(id, game::PreviousTransform{0.0f, 0.0f, 0.0f});
    world.addComponent(id, game::Velocity{0.0f, 0.0f});
    world.addComponent(id, game::PlayerInput{});

    const auto targetFrame = std::chrono::duration_cast<clock::duration>(
        secondsf(engine::Simulation::FIXED_DT));
    auto previous = clock::now();
    std::uint32_t lastLoggedTick = 0;

    while (true) {
        const auto now = clock::now();
        const float frameDt = secondsf(now - previous).count();
        previous = now;

        sim.advance(frameDt);

        if (sim.tick() / 60 > lastLoggedTick / 60) {
            engine::Logger::Info("Server tick: ", sim.tick(),
                                 " (", sim.tick() / 60, "s)");
            lastLoggedTick = sim.tick();
        }

        std::this_thread::sleep_until(now + targetFrame);
    }

    return 0;
}
