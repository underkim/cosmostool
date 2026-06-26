#pragma once

#include "ICommandExecutor.h"

namespace OpenC3::Core::Connection {

/// Null-object implementation of ICommandExecutor.
///
/// Used as a safe default before a real connection is established.
/// All operations return failure with a clear message.
/// This avoids null-pointer checks throughout the service layer.
class NullCommandExecutor final : public ICommandExecutor {
public:
    [[nodiscard]] bool connect()            override { return false; }
    void               disconnect()         override {}
    [[nodiscard]] bool isConnected() const noexcept override { return false; }

    [[nodiscard]] ExecutorResult execute(const std::string& /*cmd*/) override
    {
        return ExecutorResult::fail("Not connected to any environment");
    }

    [[nodiscard]] ExecutorResult executeStreaming(
        const std::string& /*cmd*/,
        std::function<void(const std::string&)> /*onOutput*/) override
    {
        return ExecutorResult::fail("Not connected to any environment");
    }

    [[nodiscard]] bool uploadFile(const std::string&, const std::string&)
        override { return false; }
    [[nodiscard]] bool downloadFile(const std::string&, const std::string&)
        override { return false; }
    [[nodiscard]] bool        fileExists(const std::string&) override { return false; }
    [[nodiscard]] std::string readFile(const std::string&)   override { return {}; }
    [[nodiscard]] bool writeFile(const std::string&, const std::string&)
        override { return false; }
    [[nodiscard]] std::vector<std::string> listDirectory(const std::string&)
        override { return {}; }
};

} // namespace OpenC3::Core::Connection
