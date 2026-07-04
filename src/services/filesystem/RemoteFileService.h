#pragma once

#include "IRemoteFileService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

class RemoteFileService final : public IRemoteFileService {
public:
    explicit RemoteFileService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] std::vector<std::string>
        listDirectory(const std::string& remotePath) override;

    [[nodiscard]] std::string readFile(const std::string& remotePath) override;

    [[nodiscard]] bool writeFile(const std::string& remotePath,
                                  const std::string& content) override;

    [[nodiscard]] bool        fileExists     (const std::string& remotePath) override;
    [[nodiscard]] std::string executeCommand (const std::string& command)    override;

    void streamCommand(const std::string&                      command,
                       std::function<void(const std::string&)> onLine) override;
    void cancelStreamingCommand() override;

private:
    Core::Connection::ICommandExecutor& executor_;
};

} // namespace OpenC3::Services
