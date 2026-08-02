#include "core/Logger.hpp"

#include <utility>

namespace smf::core {

Logger::Logger(logging::LoggerApi& backend, std::string scope)
    : backend_(&backend),
      scope_(std::move(scope)) {
    if (scope_.empty()) {
        scope_ = "Core";
    }
}

Logger Logger::Child(const std::string_view childScope) const {
    std::string combined = scope_;
    if (!combined.empty()) {
        combined += '/';
    }
    combined.append(childScope);
    return Logger{*backend_, std::move(combined)};
}

std::string_view Logger::Scope() const noexcept {
    return scope_;
}

void Logger::Log(
    const logging::LogLevel level,
    const std::string_view message) const {
    if (backend_ == nullptr) {
        return;
    }
    backend_->Log(level, Format(message));
}

void Logger::Trace(const std::string_view message) const {
    Log(logging::LogLevel::Trace, message);
}

void Logger::Debug(const std::string_view message) const {
    Log(logging::LogLevel::Debug, message);
}

void Logger::Info(const std::string_view message) const {
    Log(logging::LogLevel::Info, message);
}

void Logger::Warning(const std::string_view message) const {
    Log(logging::LogLevel::Warning, message);
}

void Logger::Error(const std::string_view message) const {
    Log(logging::LogLevel::Error, message);
}

void Logger::Critical(const std::string_view message) const {
    Log(logging::LogLevel::Critical, message);
}

std::string Logger::Format(const std::string_view message) const {
    std::string output;
    output.reserve(scope_.size() + message.size() + 4);
    output += '[';
    output += scope_;
    output += "] ";
    output.append(message);
    return output;
}

} // namespace smf::core
