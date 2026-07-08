#pragma once

#include "core/connection/ICommandExecutor.h"
#include "core/connection/ConnectionConfig.h"

#include <atomic>

// Forward-declare libssh2 handle to avoid polluting every translation unit
struct _LIBSSH2_SESSION;
struct _LIBSSH2_CHANNEL;

namespace OpenC3::Core::Connection {

/// Executes commands on a remote Linux server over SSH using libssh2.
///
/// Supports password and public-key authentication.
/// The socket is managed via platform BSD sockets (WinSock2 on Windows).
class SshExecutor final : public ICommandExecutor {
public:
    explicit SshExecutor(ConnectionConfig config);
    ~SshExecutor() override;

    SshExecutor(const SshExecutor&)            = delete;
    SshExecutor& operator=(const SshExecutor&) = delete;
    SshExecutor(SshExecutor&&)                 = delete;
    SshExecutor& operator=(SshExecutor&&)      = delete;

    // ── ICommandExecutor ──────────────────────────────────────────────────────
    [[nodiscard]] bool connect() override;
    void               disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] std::string lastError() const override { return lastError_; }

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
    [[nodiscard]] bool authenticatePassword();
    [[nodiscard]] bool authenticatePublicKey();
    [[nodiscard]] bool resolveAndConnect();

    [[nodiscard]] ExecutorResult runChannel(const std::string& command);

    void cleanupSession();

    ConnectionConfig   config_;
    _LIBSSH2_SESSION*  session_{nullptr};
    int                socket_{-1};
    bool               connected_{false};
    std::string        lastError_;
    std::atomic_bool   cancelStreaming_{false};
};

} // namespace OpenC3::Core::Connection
