#pragma once

#include "ICommandExecutor.h"
#include "NullCommandExecutor.h"

#include <atomic>

namespace OpenC3::Core::Connection {

/// Thread-safe executor forwarder.
///
/// Services hold a stable ExecutorProxy& whose identity never changes.
/// The proxy atomically delegates all calls to the current "real" executor.
/// swap() is called by Application when a connection is established;
/// reset() is called on disconnect or error.
class ExecutorProxy final : public ICommandExecutor {
public:
    ExecutorProxy();

    void swap(ICommandExecutor* newExecutor) noexcept;
    void reset() noexcept;

    // ── ICommandExecutor ──────────────────────────────────────────────────────
    [[nodiscard]] bool connect()            override;
    void               disconnect()         override;
    [[nodiscard]] bool isConnected() const noexcept override;

    [[nodiscard]] ExecutorResult execute(const std::string& command) override;
    [[nodiscard]] ExecutorResult executeStreaming(
        const std::string&                      command,
        std::function<void(const std::string&)> onOutput) override;

    [[nodiscard]] bool uploadFile(const std::string& localPath,
                                  const std::string& remotePath) override;
    [[nodiscard]] bool downloadFile(const std::string& remotePath,
                                    const std::string& localPath) override;
    [[nodiscard]] bool        fileExists(const std::string& remotePath) override;
    [[nodiscard]] std::string readFile(const std::string& remotePath)   override;
    [[nodiscard]] bool writeFile(const std::string& remotePath,
                                 const std::string& content) override;
    [[nodiscard]] std::vector<std::string>
        listDirectory(const std::string& remotePath) override;

private:
    NullCommandExecutor            nullExecutor_;
    std::atomic<ICommandExecutor*> current_;
};

} // namespace OpenC3::Core::Connection
