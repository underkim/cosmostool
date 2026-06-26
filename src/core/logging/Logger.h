#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

#include <memory>
#include <string>

namespace OpenC3::Core::Logging {

/// Thin façade over spdlog.
///
/// Rationale: if we ever need to swap the underlying library, only
/// this file changes. Callers never include spdlog headers directly.
///
/// Usage:
///   Logger::info("Widget {} initialised", widgetId);
///   Logger::error("[DockerService] container '{}' not found", id);
class Logger {
public:
    static void init(const std::string& logFilePath = "");

    template<typename... Args>
    static void trace(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::critical(fmt, std::forward<Args>(args)...);
    }

    static void setLevel(spdlog::level::level_enum level);
    static void flush();
};

} // namespace OpenC3::Core::Logging

// Convenience alias — most callers use the short form
namespace OpenC3 {
    namespace Logging = Core::Logging;
}
