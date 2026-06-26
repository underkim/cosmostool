#include "PluginService.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace OpenC3::Services {

PluginService::PluginService(Core::Connection::ICommandExecutor& executor)
    : executor_(executor)
{}

std::vector<Models::Plugin> PluginService::listInstalled()
{
    // OpenC3 stores installed plugin records in its Redis/Minio state.
    // We query via the openc3cli tool if available, otherwise fall back to
    // listing gem files in the plugins directory.
    auto r = executor_.execute(
        "find /cosmos/plugins -name '*.gem' -type f 2>/dev/null");

    std::vector<Models::Plugin> plugins;
    if (!r) return plugins;

    std::istringstream stream(r.stdOut);
    std::string        line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        Models::Plugin p;
        // Extract name from gem filename: name-version.gem
        const std::size_t slash = line.rfind('/');
        const std::string filename = (slash != std::string::npos)
            ? line.substr(slash + 1) : line;
        const std::size_t ext = filename.rfind(".gem");
        p.gemFilePath = line;
        p.name        = (ext != std::string::npos) ? filename.substr(0, ext) : filename;
        p.status      = Models::PluginStatus::Installed;
        plugins.push_back(std::move(p));
    }
    return plugins;
}

Models::PluginValidationResult
PluginService::validate(const std::string& localPluginPath)
{
    Models::PluginValidationResult result;
    result.valid = true;

    // Phase 7 will implement full validation logic.
    // Skeleton validates that the path exists on the local filesystem.
    auto r = executor_.execute(
        "test -f '" + localPluginPath + "' && echo FOUND || echo MISSING");

    if (!r || r.stdOut.find("MISSING") != std::string::npos) {
        Models::PluginValidationIssue issue;
        issue.severity = Models::PluginValidationIssue::Severity::Error;
        issue.code     = "FILE_NOT_FOUND";
        issue.message  = "Plugin file not found: " + localPluginPath;
        result.issues.push_back(issue);
        result.valid = false;
    }
    return result;
}

bool PluginService::install(const std::string& gemFilePath)
{
    Logging::Logger::info("[PluginService] Installing plugin: {}", gemFilePath);
    // Full implementation will use openc3cli or curl to the COSMOS API
    auto r = executor_.execute(
        "cp '" + gemFilePath + "' /cosmos/plugins/ 2>&1");
    return static_cast<bool>(r);
}

bool PluginService::remove(const std::string& pluginName)
{
    Logging::Logger::info("[PluginService] Removing plugin: {}", pluginName);
    auto r = executor_.execute(
        "rm -f /cosmos/plugins/" + pluginName + ".gem 2>&1");
    return static_cast<bool>(r);
}

bool PluginService::backup(
    const std::string& pluginName,
    const std::string& localBackupPath)
{
    return executor_.downloadFile(
        "/cosmos/plugins/" + pluginName + ".gem", localBackupPath);
}

} // namespace OpenC3::Services
