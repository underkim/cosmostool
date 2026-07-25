#pragma once

#include "models/Script.h"

#include <string>
#include <vector>

namespace OpenC3::Services {

/// Script lifecycle against a *running* COSMOS instance's Script Runner API
/// (script-api), never a local WSL/SSH shell.
///
/// WHY THIS EXISTS SEPARATELY FROM "Run Script" ON THE SHELL
/// PluginView's existing Run Script button executes `ruby <file>` directly on
/// the connected WSL/SSH host - useful as a quick syntax/smoke check, but not
/// the real COSMOS Script Runner: cmd()/tlm()/wait_check() only resolve if
/// the openc3 script libraries happen to be importable in that shell, there
/// is no pause/stop/step, and nothing is recorded as a COSMOS running or
/// completed script. This interface is the real thing: it lists scripts that
/// are actually deployed in the connected scope's bucket storage (via an
/// installed plugin - see ICosmosPluginService), starts them inside COSMOS's
/// own script_runner process, and drives them through COSMOS's own
/// ScriptStatusModel state machine (spawning -> running -> ... ->
/// completed/completed_errors/stopped/crashed/killed).
///
/// Implementations must remain safe to call before a connection exists; the
/// null API client turns every call into a clean failure, matching every
/// other Cosmos*Service in this codebase.
class IScriptService {
public:
    virtual ~IScriptService() = default;

    /// Scripts deployed in the connected scope (optionally scoped to one
    /// target). Each CosmosScript::path is a bucket-relative name such as
    /// "INST/procedures/checkout.rb" - exactly what run()/status() expect as
    /// their name/id argument, not a local filesystem path.
    [[nodiscard]] virtual std::vector<Models::CosmosScript> listScripts(
        const std::string& targetFilter = {}) = 0;

    /// Starts a script disconnected from any interactive prompt/breakpoint
    /// UI - the only mode this desktop client can meaningfully support
    /// without also implementing COSMOS's ActionCable prompt channel. Mirrors
    /// PluginOperationResult's contract: success means "COSMOS accepted and
    /// started the job", not "the script finished".
    [[nodiscard]] virtual Models::ScriptRunResult run(
        const std::string& scriptPath) = 0;

    /// Reads the current OpenC3::ScriptStatusModel for a running-script id
    /// returned by run(). Works for both still-running and already-completed
    /// scripts (ScriptStatusModel::get checks the running key first, then
    /// falls back to the completed key).
    [[nodiscard]] virtual Models::ScriptExecutionStatus status(
        const std::string& runningScriptId) = 0;

    /// Signals a running script to halt. Like install/uninstall, this starts
    /// the stop - the terminal state still arrives asynchronously through
    /// status().
    [[nodiscard]] virtual bool stop(const std::string& runningScriptId) = 0;

    /// Resumes a paused/breakpointed script.
    [[nodiscard]] virtual bool resume(const std::string& runningScriptId) = 0;

    /// Pauses a running script at its next checked line.
    [[nodiscard]] virtual bool pause(const std::string& runningScriptId) = 0;

    /// True when the underlying API client currently has a live,
    /// authenticated endpoint.
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace OpenC3::Services
