#include "engine/core/Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace engine {

std::mutex Logger::logMutex;

void Logger::Init() {
    // Could initialize file output here if needed
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Color codes
    const char* colorCode = "";
    const char* levelStr = "";
    const char* resetCode = "\033[0m";

    switch (level) {
        case LogLevel::DEBUG:
            colorCode = "\033[36m"; // Cyan
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            colorCode = "\033[32m"; // Green
            levelStr = "INFO ";
            break;
        case LogLevel::WARN:
            colorCode = "\033[33m"; // Yellow
            levelStr = "WARN ";
            break;
        case LogLevel::ERROR:
            colorCode = "\033[31m"; // Red
            levelStr = "ERROR";
            break;
    }

    // Format: [HH:MM:SS.ms] [LEVEL] Message
    std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") 
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
              << colorCode << "[" << levelStr << "] " << resetCode 
              << message << std::endl;
}

} // namespace engine
