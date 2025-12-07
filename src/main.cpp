
#include "engine/core/Engine.h"
#include "engine/core/Logger.h"

int main() {
    engine::Logger::Info("Starting Dungeon Crawler Engine...");
    Engine engine(640, 480, "Dungeon Crawler - ECS Demo");
    engine.Run();
    return 0;
}
