#include "WslExecutor.h"
#include "core/logging/Logger.h"

#include <array>
#include <cstdio>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
// popen / pclose are available on MSVC as _popen / _pclose
#  define POPEN  _popen
#  define PCLOSE _pclose
#else
#  define POPEN  popen
#  define PCLOSE pclose
#endif

namespace OpenC3::Core::Connection {

namespace {
constexpr std::size_t kReadBufferSize = 4096;
} // namespace

WslExecutor::WslExecutor(ConnectionConfig config)
    : config_(std::move(config))
{}

WslExecutor::~WslExecutor()
{
    disconnect();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool WslExecutor::connect()
{
    if (connected_) return true;

    // Verify wsl.exe is reachable and the chosen distribution exists
    const std::string probe =
        "wsl.exe -d " + config_.wslDistribution + " -- echo OK 2>&1";

    auto result = runProcess(probe);
    if (!result || result.stdOut.find("OK") == std::string::npos) {
        Logging::Logger::error(
            "[WslExecutor] connect failed for distro '{}': {}",
            config_.wslDistribution, result.errorMessage);
        return false;
    }

    connected_ = true;
    Logging::Logger::info("[WslExecutor] Connected to '{}'",
                          config_.wslDistribution);
    return true;
}

void WslExecutor::disconnect()
{
    if (!connected_) return;
    connected_ = false;
    Logging::Logger::info("[WslExecutor] Disconnected from '{}'",
                          config_.wslDistribution);
}

bool WslExecutor::isConnected() const noexcept
{
    return connected_;
}

// ── Command execution ─────────────────────────────────────────────────────────

ExecutorResult WslExecutor::execute(const std::string& command)
{
    if (!connected_) return ExecutorResult::fail("Not connected");

    const std::string fullCmd = buildWslCommand(command);
    Logging::Logger::debug("[WslExecutor] execute: {}", command);
    return runProcess(fullCmd);
}

ExecutorResult WslExecutor::executeStreaming(
    const std::string&                      command,
    std::function<void(const std::string&)> onOutput)
{
    if (!connected_) return ExecutorResult::fail("Not connected");

    const std::string fullCmd = buildWslCommand(command) + " 2>&1";
    Logging::Logger::debug("[WslExecutor] executeStreaming: {}", command);

    FILE* pipe = POPEN(fullCmd.c_str(), "r");
    if (!pipe) return ExecutorResult::fail("Failed to open pipe");

    std::string          accumulated;
    std::array<char, kReadBufferSize> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        std::string chunk(buf.data());
        accumulated += chunk;
        if (onOutput) onOutput(chunk);
    }

    const int code = PCLOSE(pipe);
    return ExecutorResult::fromProcess(code, accumulated, {});
}

// ── File operations ───────────────────────────────────────────────────────────

bool WslExecutor::uploadFile(
    const std::string& localPath,
    const std::string& remotePath)
{
    // Convert Windows path to WSL path via wslpath, then copy
    // For skeleton: use wsl.exe cp with wslpath conversion
    const std::string wslLocal =
        "$(wslpath '" + localPath + "')";
    const std::string cmd =
        "cp " + wslLocal + " " + remotePath;
    return static_cast<bool>(execute(cmd));
}

bool WslExecutor::downloadFile(
    const std::string& remotePath,
    const std::string& localPath)
{
    const std::string wslLocal =
        "$(wslpath '" + localPath + "')";
    const std::string cmd =
        "cp " + remotePath + " " + wslLocal;
    return static_cast<bool>(execute(cmd));
}

bool WslExecutor::fileExists(const std::string& remotePath)
{
    auto r = execute("test -e '" + remotePath + "' && echo 1 || echo 0");
    return r && r.stdOut.find('1') != std::string::npos;
}

std::string WslExecutor::readFile(const std::string& remotePath)
{
    auto r = execute("cat '" + remotePath + "'");
    return r ? r.stdOut : std::string{};
}

bool WslExecutor::writeFile(
    const std::string& remotePath,
    const std::string& content)
{
    // Write via heredoc to handle arbitrary content
    const std::string cmd =
        "cat > '" + remotePath + "' << 'WSLEOF'\n" + content + "\nWSLEOF";
    return static_cast<bool>(execute(cmd));
}

std::vector<std::string> WslExecutor::listDirectory(
    const std::string& remotePath)
{
    auto r = execute("ls -1 '" + remotePath + "'");
    if (!r) return {};

    std::vector<std::string> entries;
    std::istringstream       stream(r.stdOut);
    std::string              line;
    while (std::getline(stream, line)) {
        if (!line.empty()) entries.push_back(line);
    }
    return entries;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string WslExecutor::buildWslCommand(const std::string& command) const
{
    return "wsl.exe -d " + config_.wslDistribution + " -- " + command;
}

ExecutorResult WslExecutor::runProcess(const std::string& fullCommand)
{
    const std::string cmdWithRedirect = fullCommand + " 2>&1";

    FILE* pipe = POPEN(cmdWithRedirect.c_str(), "r");
    if (!pipe)
        return ExecutorResult::fail("Failed to create process: " + cmdWithRedirect);

    std::string                       output;
    std::array<char, kReadBufferSize> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr)
        output += buf.data();

    const int code = PCLOSE(pipe);
    return ExecutorResult::fromProcess(code, output, {});
}

} // namespace OpenC3::Core::Connection
