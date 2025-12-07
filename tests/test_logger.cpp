#include "doctest.h"
#include "engine/core/Logger.h"

TEST_CASE("Logger System") {
    SUBCASE("Initialization") {
        // Should not crash
        engine::Logger::Init();
        CHECK(true);
    }

    SUBCASE("Logging different levels") {
        // These will print to stdout, we are verifying they don't crash
        engine::Logger::Debug("Debug test");
        engine::Logger::Info("Info test");
        engine::Logger::Warn("Warn test");
        engine::Logger::Error("Error test");
        CHECK(true);
    }

    SUBCASE("Variadic formatting") {
        engine::Logger::Info("Formatting test: int=", 42, ", float=", 3.14f, ", string=", "hello");
        CHECK(true);
    }
}
