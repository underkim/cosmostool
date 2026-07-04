#include "RemoteFileService.h"

namespace OpenC3::Services {

RemoteFileService::RemoteFileService(Core::Connection::ICommandExecutor& executor)
    : executor_(executor)
{}

std::vector<std::string> RemoteFileService::listDirectory(const std::string& remotePath)
{
    return executor_.listDirectory(remotePath);
}

std::string RemoteFileService::readFile(const std::string& remotePath)
{
    return executor_.readFile(remotePath);
}

bool RemoteFileService::writeFile(const std::string& remotePath,
                                   const std::string& content)
{
    return executor_.writeFile(remotePath, content);
}

bool RemoteFileService::fileExists(const std::string& remotePath)
{
    return executor_.fileExists(remotePath);
}

std::string RemoteFileService::executeCommand(const std::string& command)
{
    auto result = executor_.execute(command);
    return result ? result.stdOut : std::string{};
}

void RemoteFileService::streamCommand(
    const std::string&                      command,
    std::function<void(const std::string&)> onLine)
{
    (void)executor_.executeStreaming(command, std::move(onLine));
}

void RemoteFileService::cancelStreamingCommand()
{
    executor_.cancelStreaming();
}

} // namespace OpenC3::Services
