#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace smf::logging {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class LoggerApi {
public:
    LoggerApi() = default;
    ~LoggerApi();

    LoggerApi(const LoggerApi&) = delete;
    LoggerApi& operator=(const LoggerApi&) = delete;

    bool Initialize(const std::filesystem::path& filePath);
    void Shutdown();
    void SetConsoleOutputEnabled(bool enabled) noexcept;
    void Log(LogLevel level, std::string_view message);

    void Trace(std::string_view message);
    void Debug(std::string_view message);
    void Info(std::string_view message);
    void Warning(std::string_view message);
    void Error(std::string_view message);
    void Critical(std::string_view message);

    [[nodiscard]] bool IsOpen() const;

private:
    static std::string_view LevelName(LogLevel level);

    mutable std::mutex mutex_;
    std::ofstream stream_;
    bool consoleOutputEnabled_{false};
};

} // namespace smf::logging

