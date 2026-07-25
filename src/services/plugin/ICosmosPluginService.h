#pragma once

#include "models/Plugin.h"

#include <string>
#include <vector>

namespace OpenC3::Services {

/// Outcome of a COSMOS plugin operation.
///
/// Plugin install/uninstall are asynchronous on the COSMOS side: the
/// controller spawns `openc3cli load`/`unload` through ProcessManager and
/// immediately renders the *process name*, not a completion status. So a
/// successful call here means "COSMOS accepted the request and started the
/// job", which is a genuinely weaker claim than "the plugin is installed" -
/// and one the UI must not overstate. processName carries that handle.
struct PluginOperationResult {
    bool        success{false};
    std::string message;      ///< Human-readable detail, safe to show verbatim
    std::string processName;  ///< COSMOS-side job handle, when one was started

    [[nodiscard]] static PluginOperationResult ok(
        std::string detail = {}, std::string process = {})
    {
        PluginOperationResult r;
        r.success     = true;
        r.message     = std::move(detail);
        r.processName = std::move(process);
        return r;
    }

    [[nodiscard]] static PluginOperationResult fail(std::string detail)
    {
        PluginOperationResult r;
        r.success = false;
        r.message = std::move(detail);
        return r;
    }

    explicit operator bool() const noexcept { return success; }
};

/// Snapshot of the ProcessStatusModel created by COSMOS ProcessManager.
struct PluginJobStatus {
    bool        querySucceeded{false};
    bool        found{false};
    std::string processName;
    std::string state;
    std::string detail;
    std::string output;
    std::string errorMessage;

    [[nodiscard]] bool isTerminal() const noexcept
    {
        return state == "Complete" || state == "Warning" || state == "Error"
            || state == "Crashed" || state == "Expired";
    }

    [[nodiscard]] bool completedSuccessfully() const noexcept
    {
        return state == "Complete" || state == "Warning";
    }
};

/// Plugin lifecycle against a *running* COSMOS instance, over the HTTP API.
///
/// WHY THIS EXISTS SEPARATELY FROM IPluginService
/// IPluginService treats plugins as files on the COSMOS host: it lists
/// `<root>/plugins/*` directories, "installs" by copying a .gem into that
/// folder, and "removes" by deleting it. None of that is how COSMOS works.
/// An installed plugin lives in a MinIO bucket plus Redis configuration, and
/// installing one is a two-phase server-side operation that unpacks the gem,
/// parses plugin.txt, writes models, and starts microservices. Copying a file
/// into a directory has no effect whatsoever on a running COSMOS.
///
/// So the two interfaces answer genuinely different questions and both are
/// legitimate:
///   IPluginService      - "what am I developing, and does it build?"
///                         (local/remote source folders, gem build, validate)
///   ICosmosPluginService - "what is actually installed in COSMOS right now?"
///                         (this interface)
///
/// Implementations must remain safe to call before a connection exists; the
/// null API client turns every call into a clean failure.
class ICosmosPluginService {
public:
    virtual ~ICosmosPluginService() = default;

    /// Plugins currently installed in the connected scope.
    ///
    /// Note the returned Plugin::name is COSMOS's *instance* name, which
    /// carries an installation suffix (e.g. "openc3-cosmos-demo-1.0.0.gem__20240101120000")
    /// because the same gem may be installed more than once. That full string
    /// is what every other route expects as :id.
    [[nodiscard]] virtual std::vector<Models::Plugin> listInstalled() = 0;

    /// Phase 1 of install: upload the .gem and get back the plugin hash
    /// describing its variables and plugin.txt lines. Nothing is installed
    /// yet - COSMOS only inspects the gem.
    ///
    /// Returns the raw JSON plugin hash on success, because phase 2 must send
    /// it back unmodified (it is COSMOS's own structure, not ours to model).
    [[nodiscard]] virtual PluginOperationResult uploadGem(
        const std::string& localGemPath) = 0;

    /// Phase 2 of install: hand the phase-1 plugin hash back to COSMOS, which
    /// spawns `openc3cli load` to perform the real installation.
    [[nodiscard]] virtual PluginOperationResult installUploaded(
        const std::string& pluginId,
        const std::string& pluginHashJson) = 0;

    /// Convenience wrapper running both phases in order. This is what the UI
    /// wants for a plain "install this .gem" action.
    [[nodiscard]] virtual PluginOperationResult installGem(
        const std::string& localGemPath) = 0;

    /// Uninstall by COSMOS instance name (the suffixed name from
    /// listInstalled(), not the bare gem name). Spawns `openc3cli unload`.
    [[nodiscard]] virtual PluginOperationResult uninstall(
        const std::string& pluginId) = 0;

    /// Reads the ProcessStatusModel for a processName returned by install or
    /// uninstall. A successful query may temporarily have found == false while
    /// ProcessManager is still creating the status model.
    [[nodiscard]] virtual PluginJobStatus jobStatus(
        const std::string& processName) = 0;

    /// True when the underlying API client currently has a live, authenticated
    /// endpoint. Lets the UI distinguish "nothing installed" from "we cannot
    /// see COSMOS at all" instead of showing a misleading empty list.
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace OpenC3::Services
