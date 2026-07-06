#pragma once

#include "ExecutorResult.h"

#include <functional>
#include <string>
#include <vector>

namespace OpenC3::Core::Connection {

/// The central abstraction that insulates the entire service layer
/// from whether execution happens through WSL or SSH.
///
/// Implementors: WslExecutor, SshExecutor
/// Consumers: all Service classes (via dependency injection)
///
/// All methods are synchronous. Callers that need async behaviour should
/// invoke from a worker thread (e.g. std::async, QThread, QtConcurrent).
class ICommandExecutor {
public:
    virtual ~ICommandExecutor() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Establish connection to the target environment.
    /// Returns false on failure; reason in ExecutorResult::errorMessage if
    /// callers need it (use connect2() below for that).
    [[nodiscard]] virtual bool connect() = 0;

    virtual void disconnect() = 0;

    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// Detail on why the most recent connect() call failed (empty if it
    /// succeeded, or if the implementation doesn't distinguish failure
    /// modes). Lets callers surface e.g. "auth failed" vs. "host
    /// unreachable" instead of one generic message.
    [[nodiscard]] virtual std::string lastError() const { return {}; }

    // ── Command execution ─────────────────────────────────────────────────────

    /// Execute a shell command and return combined stdout / stderr.
    [[nodiscard]] virtual ExecutorResult execute(const std::string& command) = 0;

    /// Execute with live stdout streaming. The callback is called on each
    /// newline-delimited chunk. Thread-safety of the callback is the caller's
    /// responsibility.
    [[nodiscard]] virtual ExecutorResult executeStreaming(
        const std::string&                       command,
        std::function<void(const std::string&)>  onOutput) = 0;

    /// Best-effort cancellation for any active streaming command started via
    /// executeStreaming(). Implementations that own a child process/channel
    /// should terminate it; implementations without streaming state may no-op.
    virtual void cancelStreaming() {}

    // ── File operations ───────────────────────────────────────────────────────

    [[nodiscard]] virtual bool uploadFile(
        const std::string& localPath,
        const std::string& remotePath) = 0;

    [[nodiscard]] virtual bool downloadFile(
        const std::string& remotePath,
        const std::string& localPath) = 0;

    [[nodiscard]] virtual bool        fileExists(const std::string& remotePath) = 0;
    [[nodiscard]] virtual std::string readFile(const std::string& remotePath)   = 0;

    [[nodiscard]] virtual bool writeFile(
        const std::string& remotePath,
        const std::string& content) = 0;

    [[nodiscard]] virtual std::vector<std::string> listDirectory(
        const std::string& remotePath) = 0;
};

} // namespace OpenC3::Core::Connection
