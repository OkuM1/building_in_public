#include "client/ClientApp.h"
#include "engine/core/Logger.h"

int main() {
    engine::Logger::Info("Starting Dungeon Crawler Engine...");
    client::ClientApp app(640, 480, "Dungeon Crawler - ECS Demo");
    app.Run();
    return 0;
}
