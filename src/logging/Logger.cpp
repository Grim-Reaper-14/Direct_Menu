#include "logging/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace smf::logging {

LoggerApi::~LoggerApi() {
    Shutdown();
}

bool LoggerApi::Initialize(const std::filesystem::path& filePath) {
    std::scoped_lock lock{mutex_};
    stream_.open(filePath, std::ios::out | std::ios::app);
    return stream_.is_open();
}

void LoggerApi::Shutdown() {
    std::scoped_lock lock{mutex_};
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    consoleOutputEnabled_ = false;
}

void LoggerApi::SetConsoleOutputEnabled(const bool enabled) noexcept {
    std::scoped_lock lock{mutex_};
    consoleOutputEnabled_ = enabled;
}

void LoggerApi::Log(const LogLevel level, const std::string_view message) {
    std::scoped_lock lock{mutex_};
    if (!stream_.is_open() && !consoleOutputEnabled_) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &currentTime);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
        std::chrono::seconds{1};

    std::ostringstream line;
    line
        << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << milliseconds.count()
        << " [" << LevelName(level) << ']'
        << " [thread " << std::this_thread::get_id() << "] "
        << message << '\n';

    const std::string formatted = line.str();
    if (stream_.is_open()) {
        stream_ << formatted;
        stream_.flush();
    }
    if (consoleOutputEnabled_) {
        std::clog << formatted;
        std::clog.flush();
    }
}

void LoggerApi::Trace(const std::string_view message) {
    Log(LogLevel::Trace, message);
}

void LoggerApi::Debug(const std::string_view message) {
    Log(LogLevel::Debug, message);
}

void LoggerApi::Info(const std::string_view message) {
    Log(LogLevel::Info, message);
}

void LoggerApi::Warning(const std::string_view message) {
    Log(LogLevel::Warning, message);
}

void LoggerApi::Error(const std::string_view message) {
    Log(LogLevel::Error, message);
}

void LoggerApi::Critical(const std::string_view message) {
    Log(LogLevel::Critical, message);
}

bool LoggerApi::IsOpen() const {
    std::scoped_lock lock{mutex_};
    return stream_.is_open();
}

std::string_view LoggerApi::LevelName(const LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Critical:
        return "CRITICAL";
    }
    return "UNKNOWN";
}

} // namespace smf::logging
