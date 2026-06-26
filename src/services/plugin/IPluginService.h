#pragma once

#include "models/Plugin.h"
#include <string>
#include <vector>

namespace OpenC3::Services {

class IPluginService {
public:
    virtual ~IPluginService() = default;

    [[nodiscard]] virtual std::vector<Models::Plugin> listInstalled() = 0;

    [[nodiscard]] virtual Models::PluginValidationResult
        validate(const std::string& localPluginPath) = 0;

    [[nodiscard]] virtual bool install(const std::string& gemFilePath) = 0;
    [[nodiscard]] virtual bool remove(const std::string& pluginName)   = 0;

    [[nodiscard]] virtual bool backup(
        const std::string& pluginName,
        const std::string& localBackupPath) = 0;
};

} // namespace OpenC3::Services
