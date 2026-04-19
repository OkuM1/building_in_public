#include "engine/core/Simulation.h"

/**
 * @file Simulation.cpp
 * @brief Implementation of @ref engine::Simulation. No platform deps.
 * @see docs/design/0003-extract-simulation.md
 */

namespace engine {

void Simulation::addSystem(std::unique_ptr<System> system) {
    systems_.push_back(std::move(system));
}

void Simulation::step() {
    for (auto& system : systems_) {
        system->update(world_, FIXED_DT);
    }
    ++tick_;
}

void Simulation::advance(float realDt) {
    // Cap to avoid the spiral of death after a long stall (e.g. debugger
    // breakpoint). 0.25 s matches Glenn Fiedler's recommendation.
    if (realDt > 0.25f) realDt = 0.25f;

    accumulator_ += realDt;
    while (accumulator_ >= FIXED_DT) {
        step();
        accumulator_ -= FIXED_DT;
    }
}

} // namespace engine
