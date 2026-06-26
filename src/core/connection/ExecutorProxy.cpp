#include "ExecutorProxy.h"

namespace OpenC3::Core::Connection {

ExecutorProxy::ExecutorProxy()
    : current_{&nullExecutor_}
{}

void ExecutorProxy::swap(ICommandExecutor* newExecutor) noexcept
{
    current_.store(newExecutor ? newExecutor : &nullExecutor_,
                   std::memory_order_release);
}

void ExecutorProxy::reset() noexcept
{
    current_.store(&nullExecutor_, std::memory_order_release);
}

bool ExecutorProxy::connect()
{
    return current_.load(std::memory_order_acquire)->connect();
}

void ExecutorProxy::disconnect()
{
    current_.load(std::memory_order_acquire)->disconnect();
}

bool ExecutorProxy::isConnected() const noexcept
{
    return current_.load(std::memory_order_acquire)->isConnected();
}

ExecutorResult ExecutorProxy::execute(const std::string& command)
{
    return current_.load(std::memory_order_acquire)->execute(command);
}

ExecutorResult ExecutorProxy::executeStreaming(
    const std::string&                      command,
    std::function<void(const std::string&)> onOutput)
{
    return current_.load(std::memory_order_acquire)
                   ->executeStreaming(command, std::move(onOutput));
}

bool ExecutorProxy::uploadFile(const std::string& localPath,
                               const std::string& remotePath)
{
    return current_.load(std::memory_order_acquire)
                   ->uploadFile(localPath, remotePath);
}

bool ExecutorProxy::downloadFile(const std::string& remotePath,
                                 const std::string& localPath)
{
    return current_.load(std::memory_order_acquire)
                   ->downloadFile(remotePath, localPath);
}

bool ExecutorProxy::fileExists(const std::string& remotePath)
{
    return current_.load(std::memory_order_acquire)->fileExists(remotePath);
}

std::string ExecutorProxy::readFile(const std::string& remotePath)
{
    return current_.load(std::memory_order_acquire)->readFile(remotePath);
}

bool ExecutorProxy::writeFile(const std::string& remotePath,
                              const std::string& content)
{
    return current_.load(std::memory_order_acquire)
                   ->writeFile(remotePath, content);
}

std::vector<std::string> ExecutorProxy::listDirectory(const std::string& remotePath)
{
    return current_.load(std::memory_order_acquire)->listDirectory(remotePath);
}

} // namespace OpenC3::Core::Connection
