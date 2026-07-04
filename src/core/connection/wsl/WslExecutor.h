#pragma once

#include "core/connection/ICommandExecutor.h"
#include "core/connection/ConnectionConfig.h"

#include <functional>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace OpenC3::Core::Connection {

/// Executes commands inside a WSL distribution on Windows.
///
/// Implementation note:
///   Uses CreateProcess / _popen to invoke "wsl.exe -d <distro> -- <cmd>".
///   The connection "state" is validated by probing wsl.exe availability and
///   the distribution name on connect().
class WslExecutor final : public ICommandExecutor {
public:
    explicit WslExecutor(ConnectionConfig config);
    ~WslExecutor() override;

    // Non-copyable, movable
    WslExecutor(const WslExecutor&)            = delete;
    WslExecutor& operator=(const WslExecutor&) = delete;
    WslExecutor(WslExecutor&&)                 = default;
    WslExecutor& operator=(WslExecutor&&)      = default;

    // ── ICommandExecutor ──────────────────────────────────────────────────────
    [[nodiscard]] bool connect() override;
    void               disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;

    [[nodiscard]] ExecutorResult execute(const std::string& command) override;

    [[nodiscard]] ExecutorResult executeStreaming(
        const std::string&                      command,
        std::function<void(const std::string&)> onOutput) override;
    void cancelStreaming() override;

    [[nodiscard]] bool uploadFile(
        const std::string& localPath,
        const std::string& remotePath) override;

    [[nodiscard]] bool downloadFile(
        const std::string& remotePath,
        const std::string& localPath) override;

    [[nodiscard]] bool        fileExists(const std::string& remotePath) override;
    [[nodiscard]] std::string readFile(const std::string& remotePath) override;

    [[nodiscard]] bool writeFile(
        const std::string& remotePath,
        const std::string& content) override;

    [[nodiscard]] std::vector<std::string> listDirectory(
        const std::string& remotePath) override;

private:
    [[nodiscard]] std::string    buildWslCommand(const std::string& command) const;
    [[nodiscard]] ExecutorResult runProcess(const std::string& fullCommand);
    [[nodiscard]] ExecutorResult runProcessStreaming(
        const std::string&                      fullCommand,
        std::function<void(const std::string&)> onOutput);

    ConnectionConfig config_;
    bool             connected_{false};
    std::atomic_bool cancelStreaming_{false};

#ifdef _WIN32
    mutable std::mutex activeProcessMutex_;
    std::vector<void*> activeStreamingProcesses_;
#endif
};

} // namespace OpenC3::Core::Connection
