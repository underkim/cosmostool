#pragma once

#include <string>

namespace OpenC3::Core::Connection {

/// Result returned by every ICommandExecutor operation.
/// Provides value-or-error semantics without exceptions.
struct ExecutorResult {
    bool        success{false};
    int         exitCode{0};
    std::string stdOut;
    std::string stdErr;
    std::string errorMessage;

    // ── Factory helpers ───────────────────────────────────────────────────────
    [[nodiscard]] static ExecutorResult ok(std::string output = {}, int code = 0) {
        ExecutorResult r;
        r.success  = true;
        r.exitCode = code;
        r.stdOut   = std::move(output);
        return r;
    }

    [[nodiscard]] static ExecutorResult fail(std::string error, int code = -1) {
        ExecutorResult r;
        r.success      = false;
        r.exitCode     = code;
        r.errorMessage = std::move(error);
        return r;
    }

    [[nodiscard]] static ExecutorResult fromProcess(
        int code, std::string out, std::string err)
    {
        ExecutorResult r;
        r.success  = (code == 0);
        r.exitCode = code;
        r.stdOut   = std::move(out);
        r.stdErr   = std::move(err);
        if (!r.success && r.stdErr.empty())
            r.errorMessage = "Process exited with code " + std::to_string(code);
        else
            r.errorMessage = r.stdErr;
        return r;
    }

    explicit operator bool() const noexcept { return success; }
};

} // namespace OpenC3::Core::Connection
