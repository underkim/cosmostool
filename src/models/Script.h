#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

/// A script deployed inside a COSMOS scope's storage bucket, as returned by
/// GET /script-api/scripts (Script.all -> TargetFile.all). `path` is a
/// bucket-relative name such as "INST/procedures/checkout.rb" - the same
/// string /scripts/*name/run and every other script-api route expects for
/// :name. This is NOT a local WSL/SSH filesystem path and NOT the same
/// namespace as IWorkspaceService's plugin source folders: a script only
/// shows up here once its plugin has actually been installed into COSMOS
/// (see ICosmosPluginService), independent of whether the matching source
/// folder still exists on the development host.
struct CosmosScript {
    std::string path;
};

/// Outcome of starting a script. COSMOS spawns a subprocess and returns a
/// numeric running-script id immediately - this mirrors
/// Services::PluginOperationResult's "job accepted, not finished" contract
/// rather than claiming the script completed.
struct ScriptRunResult {
    bool        success{false};
    std::string runningScriptId;  ///< COSMOS's numeric id, as returned verbatim
    std::string message;

    [[nodiscard]] static ScriptRunResult ok(std::string id)
    {
        ScriptRunResult r;
        r.success         = true;
        r.runningScriptId = std::move(id);
        return r;
    }

    [[nodiscard]] static ScriptRunResult fail(std::string detail)
    {
        ScriptRunResult r;
        r.message = std::move(detail);
        return r;
    }

    explicit operator bool() const noexcept { return success; }
};

/// Snapshot of OpenC3::ScriptStatusModel for one running or completed script.
/// `state` is one of COSMOS's own strings, taken from
/// running_script.rb/script_status_model.rb rather than reinvented here:
/// spawning, init, running, paused, waiting, breakpoint, error, crashed,
/// stopped, completed, completed_errors, killed.
struct ScriptExecutionStatus {
    bool        querySucceeded{false};
    bool        found{false};
    std::string id;
    std::string state;
    std::string filename;
    std::string currentFilename;
    int         lineNo{0};
    std::string output;     ///< Concatenated from `errors`/log fields when present
    std::string errorMessage;

    [[nodiscard]] bool isTerminal() const noexcept
    {
        return state == "completed" || state == "completed_errors"
            || state == "stopped" || state == "crashed" || state == "killed";
    }

    [[nodiscard]] bool completedSuccessfully() const noexcept
    {
        return state == "completed";
    }
};

} // namespace OpenC3::Models
