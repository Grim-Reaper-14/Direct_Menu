#pragma once

#include "logging/Logger.hpp"

#include <string>
#include <string_view>

namespace smf::core {

// Core-facing logger wrapper. It reuses the single logging::LoggerApi sink
// while automatically prefixing messages with a subsystem scope.
class Logger final {
public:
    explicit Logger(
        logging::LoggerApi& backend,
        std::string scope = "Core");

    [[nodiscard]] Logger Child(std::string_view childScope) const;
    [[nodiscard]] std::string_view Scope() const noexcept;

    void Log(logging::LogLevel level, std::string_view message) const;
    void Trace(std::string_view message) const;
    void Debug(std::string_view message) const;
    void Info(std::string_view message) const;
    void Warning(std::string_view message) const;
    void Error(std::string_view message) const;
    void Critical(std::string_view message) const;

private:
    [[nodiscard]] std::string Format(std::string_view message) const;

    logging::LoggerApi* backend_{nullptr};
    std::string scope_;
};

} // namespace smf::core
