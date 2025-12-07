#pragma once

#include <string>
#include <mutex>
#include <sstream>
#include <iostream>

namespace engine {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static void Init();
    static void Log(LogLevel level, const std::string& message);
    
    template<typename... Args>
    static void Debug(Args... args) {
        Log(LogLevel::DEBUG, Format(args...));
    }

    template<typename... Args>
    static void Info(Args... args) {
        Log(LogLevel::INFO, Format(args...));
    }

    template<typename... Args>
    static void Warn(Args... args) {
        Log(LogLevel::WARN, Format(args...));
    }

    template<typename... Args>
    static void Error(Args... args) {
        Log(LogLevel::ERROR, Format(args...));
    }

private:
    static std::mutex logMutex;
    
    // Base case for recursion
    static void FormatHelper(std::stringstream&) {}

    // Recursive variadic template to stream arguments
    template<typename T, typename... Args>
    static void FormatHelper(std::stringstream& ss, T first, Args... args) {
        ss << first;
        FormatHelper(ss, args...);
    }

    template<typename... Args>
    static std::string Format(Args... args) {
        std::stringstream ss;
        FormatHelper(ss, args...);
        return ss.str();
    }
};

} // namespace engine
