#include "WslExecutor.h"
#include "core/logging/Logger.h"
#include "core/connection/ShellQuote.h"

#include <algorithm>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace OpenC3::Core::Connection {

namespace {
// Quote a single argument per the Windows CreateProcess command-line parsing
// rules. This is distinct from shellQuote(): shellQuote() protects the inner
// POSIX command handed to WSL/bash, while this protects config_.wslDistribution
// (a user-editable field) when it is spliced into the outer Win32 command line
// that CreateProcessA parses before wsl.exe even runs.
std::string quoteWindowsArg(const std::string& arg)
{
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos)
        return arg;

    std::string result = "\"";
    for (auto it = arg.begin();; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == '\\') { ++backslashes; ++it; }
        if (it == arg.end()) {
            result.append(backslashes * 2, '\\');
            break;
        } else if (*it == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result += *it;
        } else {
            result.append(backslashes, '\\');
            result += *it;
        }
    }
    result += '"';
    return result;
}
} // namespace

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
    std::function<void(const std::string&)> onOutput,
    std::atomic_bool&                       cancelStreaming,
    std::function<void(HANDLE)>             onProcessStarted,
    std::function<void(HANDLE)>             onProcessFinished)
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

    if (onProcessStarted) onProcessStarted(pi.hProcess);

    std::string accumulated;
    char        buf[4096];
    DWORD       bytesRead = 0;
    while (!cancelStreaming.load(std::memory_order_acquire)
           && ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr)
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
    if (onProcessFinished) onProcessFinished(pi.hProcess);
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
    cancelStreaming_.store(false, std::memory_order_release);

    const std::string probe =
        "wsl.exe -d " + quoteWindowsArg(config_.wslDistribution) + " -- echo OK";
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
    cancelStreaming();
    connected_ = false;
    Logging::Logger::info("[WslExecutor] Disconnected from '{}'",
                          config_.wslDistribution);
}

void WslExecutor::cancelStreaming()
{
    cancelStreaming_.store(true, std::memory_order_release);
#ifdef _WIN32
    std::vector<void*> handles;
    {
        std::lock_guard<std::mutex> lock(activeProcessMutex_);
        handles = activeStreamingProcesses_;
    }
    for (void* handle : handles) {
        if (handle) {
            // Long-running log commands (tail -f, journalctl -f, docker logs
            // -f) do not exit just because the UI stops listening. Terminating
            // the active wsl.exe process ensures closing the app cannot leave a
            // hidden streaming process behind.
            TerminateProcess(static_cast<HANDLE>(handle), 1);
        }
    }
#endif
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
    cancelStreaming_.store(false, std::memory_order_release);
    Logging::Logger::debug("[WslExecutor] streaming: {}", command);
    return runProcessStreaming(buildWslCommand(command), std::move(onOutput));
}

bool WslExecutor::uploadFile(const std::string& localPath,
                             const std::string& remotePath)
{
    const std::string wslLocal = "$(wslpath " + shellQuote(localPath) + ")";
    return static_cast<bool>(execute("cp " + wslLocal + " " + shellQuote(remotePath)));
}

bool WslExecutor::downloadFile(const std::string& remotePath,
                               const std::string& localPath)
{
    const std::string wslLocal = "$(wslpath " + shellQuote(localPath) + ")";
    return static_cast<bool>(execute("cp " + shellQuote(remotePath) + " " + wslLocal));
}

bool WslExecutor::fileExists(const std::string& remotePath)
{
    auto r = execute("test -e " + shellQuote(remotePath) + " && echo 1 || echo 0");
    return r && r.stdOut.find('1') != std::string::npos;
}

std::string WslExecutor::readFile(const std::string& remotePath)
{
    auto r = execute("cat " + shellQuote(remotePath));
    return r ? r.stdOut : std::string{};
}

bool WslExecutor::writeFile(const std::string& remotePath,
                            const std::string& content)
{
    // Fail closed if the payload could close the heredoc early (see SshExecutor).
    if (contentEndsHeredoc(content, "WSLEOF"))
        return false;

    const std::string cmd =
        "cat > " + shellQuote(remotePath) + " << 'WSLEOF'\n" + content + "\nWSLEOF";
    return static_cast<bool>(execute(cmd));
}

std::vector<std::string> WslExecutor::listDirectory(const std::string& remotePath)
{
    auto r = execute("ls -1 " + shellQuote(remotePath));
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
    return "wsl.exe -d " + quoteWindowsArg(config_.wslDistribution) + " -- " + command;
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
    return runWin32ProcessStreaming(
        fullCommand,
        std::move(onOutput),
        cancelStreaming_,
        [this](HANDLE handle) {
            std::lock_guard<std::mutex> lock(activeProcessMutex_);
            activeStreamingProcesses_.push_back(handle);
        },
        [this](HANDLE handle) {
            std::lock_guard<std::mutex> lock(activeProcessMutex_);
            activeStreamingProcesses_.erase(
                std::remove(activeStreamingProcesses_.begin(),
                            activeStreamingProcesses_.end(),
                            static_cast<void*>(handle)),
                activeStreamingProcesses_.end());
        });
#else
    FILE* pipe = popen((fullCommand + " 2>&1").c_str(), "r");
    if (!pipe) return ExecutorResult::fail("popen failed");
    std::string out;
    char buf[4096];
    while (!cancelStreaming_.load(std::memory_order_acquire)
           && fgets(buf, sizeof(buf), pipe)) {
        out += buf;
        if (onOutput) onOutput(std::string(buf));
    }
    int code = pclose(pipe);
    return ExecutorResult::fromProcess(code, out, {});
#endif
}

} // namespace OpenC3::Core::Connection
