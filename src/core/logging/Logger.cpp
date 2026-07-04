#include "Logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdio>
#include <vector>

namespace OpenC3::Core::Logging {

void Logger::init(const std::string& logFilePath)
{
    std::vector<spdlog::sink_ptr> sinks;

    // Always write to coloured console
    sinks.push_back(
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    // Optionally rotate to a file (max 5 MB × 3 files). The sink constructor
    // throws spdlog::spdlog_ex if the log directory cannot be created/opened
    // (read-only portable media, a restricted/unavailable profile directory,
    // etc.). File logging is a convenience, not a requirement for the app to
    // run, so fall back to console-only logging instead of letting an
    // uncaught exception here abort the process before any window is shown.
    if (!logFilePath.empty()) {
        try {
            sinks.push_back(
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    logFilePath,
                    5 * 1024 * 1024,  // 5 MB
                    3));
        } catch (const std::exception& e) {
            std::fprintf(stderr,
                "[Logger] File logging disabled (could not open '%s'): %s\n",
                logFilePath.c_str(), e.what());
        }
    }

    auto logger = std::make_shared<spdlog::logger>(
        "opencosmos", sinks.begin(), sinks.end());

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-8l%$] [%t] %v");

#ifdef NDEBUG
    logger->set_level(spdlog::level::info);
#else
    logger->set_level(spdlog::level::debug);
#endif

    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::warn);
}

void Logger::setLevel(spdlog::level::level_enum level)
{
    spdlog::set_level(level);
}

void Logger::flush()
{
    spdlog::default_logger()->flush();
}

} // namespace OpenC3::Core::Logging
