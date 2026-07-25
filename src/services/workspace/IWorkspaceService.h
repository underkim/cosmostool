#pragma once

#include "models/Plugin.h"

#include <string>
#include <vector>

namespace OpenC3::Services {

/// Plugin *sources* on the connected host - what the developer is working on.
///
/// WHY THIS EXISTS SEPARATELY FROM ICosmosPluginService
/// These two answer genuinely different questions and neither substitutes for
/// the other:
///
///   IWorkspaceService     "what plugin source folders are on this host?"
///                         Filesystem truth, via the shell transport. Present
///                         whether or not COSMOS is running - this is what the
///                         Plugin Creation workflow edits and builds.
///
///   ICosmosPluginService  "what is actually installed in COSMOS right now?"
///                         Redis/MinIO truth, via the HTTP API. A plugin can
///                         be installed with no source folder here (installed
///                         from a .gem elsewhere), and a source folder here
///                         may never have been installed at all.
///
/// This interface was split out of IPluginService, whose listInstalled() name
/// claimed to report installed plugins while in fact scanning
/// `<cosmosRoot>/plugins/*` on disk. That conflation is why the toolkit
/// previously showed development folders as though they were the live COSMOS
/// state. Nothing here should ever be described as "installed".
class IWorkspaceService {
public:
    virtual ~IWorkspaceService() = default;

    /// Plugin source folders under `<cosmosRoot>/plugins`.
    ///
    /// A folder qualifies when it contains plugin.txt and a .gemspec; entries
    /// missing either are still returned, flagged PluginStatus::Error, so the
    /// UI can tell the user *why* a folder is not usable rather than silently
    /// hiding it.
    ///
    /// Returns Plugin records whose rootPath/pluginTxtPath/gemspecPath point
    /// at real files on the host - unlike the COSMOS API, where an installed
    /// plugin has no host path at all.
    [[nodiscard]] virtual std::vector<Models::Plugin>
        listSources(const std::string& cosmosRoot) = 0;

    /// Static structural check of a plugin source folder: plugin.txt present,
    /// a .gemspec present, targets/ present.
    [[nodiscard]] virtual Models::PluginValidationResult
        validate(const std::string& localPluginPath) = 0;

    /// Runs `gem build` in a plugin source folder and returns the combined
    /// output. Legitimately shell work - building a gem is a local Ruby
    /// toolchain operation, not a COSMOS one.
    ///
    /// The returned text is also the error channel: this interface has no
    /// separate success flag, so failures are reported as output containing
    /// "ERROR". Callers sniff for that (see PluginViewModel::build).
    [[nodiscard]] virtual std::string build(
        const std::string& pluginRootPath) = 0;

    /// Copies a built .gem off the host to a local path.
    [[nodiscard]] virtual bool backup(
        const std::string& pluginName,
        const std::string& localBackupPath,
        const std::string& cosmosRoot) = 0;
};

} // namespace OpenC3::Services
