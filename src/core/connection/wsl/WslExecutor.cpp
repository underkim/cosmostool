#include "WslExecutor.h"
#include "core/logging/Logger.h"

#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace OpenC3::Core::Connection {

// ── Windows-only pipe helper ──────────────────────────────────────────────────
// Uses CreateProcess with CREATE_NO_WINDOW so no console flashes in a GUI app.

#ifdef _WIN32
static ExecutorResult runWin32Process(const std::string& commandLine)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;

    HANDLE hReadPipe  = nullptr;
    HANDLE hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return ExecutorResult::fail("CreatePipe failed");

    // Child must not inherit the read end
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe; // merge stderr into stdout

    PROCESS_INFORMATION pi{};

    std::string cmd = commandLine; // CreateProcessA needs non-const buffer
    const BOOL ok = CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,            // process security
        nullptr,            // thread security
        TRUE,               // inherit handles
        CREATE_NO_WINDOW,   // ← KEY: no console flash
        nullptr,            // inherit environment
        nullptr,            // inherit working directory
        &si,
        &pi);

    CloseHandle(hWritePipe); // parent closes write end so ReadFile sees EOF

    if (!ok) {
        CloseHandle(hReadPipe);
        return ExecutorResult::fail("CreateProcess failed: " + commandLine);
    }

    std::string output;
    char        buf[4096];
    DWORD       bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr)
           && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return ExecutorResult::fromProcess(static_cast<int>(exitCode), output, {});
}

static ExecutorResult runWin32ProcessStreaming(
    const std::string&                      commandLine,
    std::function<void(const std::string&)> onOutput)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return ExecutorResult::fail("CreatePipe failed");
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow  = SW_HIDE;
    si.hStdOutput  = hWritePipe;
    si.hStdError   = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmd = commandLine;
    const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                                   TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                   &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        return ExecutorResult::fail("CreateProcess failed");
    }

    std::string accumulated;
    char        buf[4096];
    DWORD       bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr)
           && bytesRead > 0) {
        buf[bytesRead] = '\0';
        std::string chunk(buf, bytesRead);
        accumulated += chunk;
        if (onOutput) onOutput(chunk);
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return ExecutorResult::fromProcess(static_cast<int>(exitCode), accumulated, {});
}
#endif // _WIN32

// ── WslExecutor ───────────────────────────────────────────────────────────────

WslExecutor::WslExecutor(ConnectionConfig config)
    : config_(std::move(config))
{}

WslExecutor::~WslExecutor()
{
    disconnect();
}

bool WslExecutor::connect()
{
    if (connected_) return true;

    const std::string probe =
        "wsl.exe -d " + config_.wslDistribution + " -- echo OK";
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

bool WslExecutor::isConnected() const noexcept { return connected_; }

ExecutorResult WslExecutor::execute(const std::string& command)
{
    if (!connected_) return ExecutorResult::fail("Not connected");
    Logging::Logger::debug("[WslExecutor] execute: {}", command);
    return runProcess(buildWslCommand(command));
}

ExecutorResult WslExecutor::executeStreaming(
    const std::string&                      command,
    std::function<void(const std::string&)> onOutput)
{
    if (!connected_) return ExecutorResult::fail("Not connected");
    Logging::Logger::debug("[WslExecutor] streaming: {}", command);
    return runProcessStreaming(buildWslCommand(command), std::move(onOutput));
}

bool WslExecutor::uploadFile(const std::string& localPath,
                             const std::string& remotePath)
{
    const std::string wslLocal = "$(wslpath '" + localPath + "')";
    return static_cast<bool>(execute("cp " + wslLocal + " " + remotePath));
}

bool WslExecutor::downloadFile(const std::string& remotePath,
                               const std::string& localPath)
{
    const std::string wslLocal = "$(wslpath '" + localPath + "')";
    return static_cast<bool>(execute("cp " + remotePath + " " + wslLocal));
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

bool WslExecutor::writeFile(const std::string& remotePath,
                            const std::string& content)
{
    const std::string cmd =
        "cat > '" + remotePath + "' << 'WSLEOF'\n" + content + "\nWSLEOF";
    return static_cast<bool>(execute(cmd));
}

std::vector<std::string> WslExecutor::listDirectory(const std::string& remotePath)
{
    auto r = execute("ls -1 '" + remotePath + "'");
    if (!r) return {};
    std::vector<std::string> entries;
    std::istringstream stream(r.stdOut);
    std::string line;
    while (std::getline(stream, line))
        if (!line.empty()) entries.push_back(line);
    return entries;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string WslExecutor::buildWslCommand(const std::string& command) const
{
    return "wsl.exe -d " + config_.wslDistribution + " -- " + command;
}

ExecutorResult WslExecutor::runProcess(const std::string& fullCommand)
{
#ifdef _WIN32
    return runWin32Process(fullCommand);
#else
    // Non-Windows fallback (shouldn't happen for this app)
    FILE* pipe = popen((fullCommand + " 2>&1").c_str(), "r");
    if (!pipe) return ExecutorResult::fail("popen failed");
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    int code = pclose(pipe);
    return ExecutorResult::fromProcess(code, out, {});
#endif
}

ExecutorResult WslExecutor::runProcessStreaming(
    const std::string&                      fullCommand,
    std::function<void(const std::string&)> onOutput)
{
#ifdef _WIN32
    return runWin32ProcessStreaming(fullCommand, std::move(onOutput));
#else
    FILE* pipe = popen((fullCommand + " 2>&1").c_str(), "r");
    if (!pipe) return ExecutorResult::fail("popen failed");
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        out += buf;
        if (onOutput) onOutput(std::string(buf));
    }
    int code = pclose(pipe);
    return ExecutorResult::fromProcess(code, out, {});
#endif
}

} // namespace OpenC3::Core::Connection
