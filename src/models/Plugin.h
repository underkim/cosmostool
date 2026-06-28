#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

enum class PluginStatus {
    Installed,
    Installing,
    UpdateAvailable,
    Error,
    Unknown
};

struct PluginTarget {
    std::string name;
    std::string interfaceType;  // e.g. "SERIAL", "TCPIP_CLIENT"
};

struct Plugin {
    std::string  name;
    std::string  version;
    std::string  description;
    std::string  author;
    std::string  cosmosVersion;     // Minimum compatible COSMOS version
    PluginStatus status{PluginStatus::Unknown};
    std::string  rootPath;           // Root directory of a development plugin
    std::string  gemFilePath;       // Path to .gem file on remote
    std::string  gemspecPath;       // Path to .gemspec file on remote
    std::string  pluginTxtPath;     // Path to plugin.txt

    std::vector<PluginTarget> targets;

    [[nodiscard]] bool isInstalled() const noexcept {
        return status == PluginStatus::Installed
            || status == PluginStatus::UpdateAvailable;
    }
};

struct PluginValidationIssue {
    enum class Severity { Error, Warning, Info };
    Severity    severity{Severity::Error};
    std::string code;    // e.g. "MISSING_PLUGIN_TXT"
    std::string message;
    std::string filePath;
    int         line{0};
};

struct PluginValidationResult {
    bool                              valid{false};
    std::vector<PluginValidationIssue> issues;
};

} // namespace OpenC3::Models
