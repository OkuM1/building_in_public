
#include "engine/core/Engine.h"
#include "engine/ecs/World.h"
#include <iostream>

struct Position {
    float x, y;
};

void testECS() {
    std::cout << "=== Testing ECS ===" << std::endl;
    engine::World world;
    
    world.registerComponent<Position>();
    
    engine::EntityId entity = world.createEntity();
    world.addComponent(entity, Position{10.0f, 20.0f});
    
    Position& pos = world.getComponent<Position>(entity);
    std::cout << "Entity " << entity << " Position: (" << pos.x << ", " << pos.y << ")" << std::endl;
    
    std::cout << "ECS Test Passed! ✓" << std::endl;
    std::cout << "===================" << std::endl << std::endl;
}

int main() {
    testECS();

    std::cout << "Starting Dungeon Crawler Engine..." << std::endl;
    Engine engine(640, 480, "Dungeon Crawler - ECS Demo");
    engine.Run();
    return 0;
}
