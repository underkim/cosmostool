#include "SshExecutor.h"
#include "core/logging/Logger.h"

#include <libssh2.h>
#include <sstream>
#include <stdexcept>
#include <array>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using SocketType = SOCKET;
   constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using SocketType = int;
   constexpr SocketType kInvalidSocket = -1;
   inline void closesocket(int s) { close(s); }
#endif

namespace OpenC3::Core::Connection {

namespace {
constexpr std::size_t kSshReadBuffer = 8192;
} // namespace

SshExecutor::SshExecutor(ConnectionConfig config)
    : config_(std::move(config))
{
    static bool libssh2_initialized = false;
    if (!libssh2_initialized) {
        if (libssh2_init(0) != 0)
            throw std::runtime_error("libssh2_init() failed");
        libssh2_initialized = true;
    }
}

SshExecutor::~SshExecutor()
{
    disconnect();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool SshExecutor::connect()
{
    if (connected_) return true;

    if (!resolveAndConnect()) return false;

    session_ = libssh2_session_init();
    if (!session_) {
        Logging::Logger::error("[SshExecutor] libssh2_session_init failed");
        closesocket(static_cast<SocketType>(socket_));
        socket_ = static_cast<int>(kInvalidSocket);
        return false;
    }

    libssh2_session_set_blocking(session_, 1);

    if (libssh2_session_handshake(session_, socket_) != 0) {
        Logging::Logger::error("[SshExecutor] Handshake failed with {}:{}",
                               config_.host, config_.port);
        cleanupSession();
        return false;
    }

    bool authenticated = (config_.authMethod == ConnectionConfig::AuthMethod::Password)
        ? authenticatePassword()
        : authenticatePublicKey();

    if (!authenticated) {
        cleanupSession();
        return false;
    }

    connected_ = true;
    Logging::Logger::info("[SshExecutor] Connected to {}@{}:{}",
                          config_.username, config_.host, config_.port);
    return true;
}

void SshExecutor::disconnect()
{
    if (!connected_) return;
    cleanupSession();
    connected_ = false;
    Logging::Logger::info("[SshExecutor] Disconnected from {}",
                          config_.host);
}

bool SshExecutor::isConnected() const noexcept { return connected_; }

// ── Command execution ─────────────────────────────────────────────────────────

ExecutorResult SshExecutor::execute(const std::string& command)
{
    if (!connected_) return ExecutorResult::fail("Not connected");
    Logging::Logger::debug("[SshExecutor] execute: {}", command);
    return runChannel(command);
}

ExecutorResult SshExecutor::executeStreaming(
    const std::string&                      command,
    std::function<void(const std::string&)> onOutput)
{
    if (!connected_) return ExecutorResult::fail("Not connected");

    LIBSSH2_CHANNEL* channel =
        libssh2_channel_open_session(session_);
    if (!channel)
        return ExecutorResult::fail("Failed to open SSH channel");

    if (libssh2_channel_exec(channel, command.c_str()) != 0) {
        libssh2_channel_free(channel);
        return ExecutorResult::fail("Failed to exec command: " + command);
    }

    std::string                       output;
    std::array<char, kSshReadBuffer>  buf{};
    ssize_t                           nread;

    while ((nread = libssh2_channel_read(channel, buf.data(),
                                          buf.size())) > 0) {
        std::string chunk(buf.data(), static_cast<std::size_t>(nread));
        output += chunk;
        if (onOutput) onOutput(chunk);
    }

    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    const int exitCode = libssh2_channel_get_exit_status(channel);
    libssh2_channel_free(channel);

    return ExecutorResult::fromProcess(exitCode, output, {});
}

// ── File operations ───────────────────────────────────────────────────────────

bool SshExecutor::uploadFile(
    const std::string& localPath,
    const std::string& remotePath)
{
    // Full SFTP implementation belongs in a future sprint.
    // For now delegate to scp-style channel command with base64 encoding.
    auto r = execute("cat > '" + remotePath + "'");
    (void)localPath;
    return static_cast<bool>(r); // TODO: implement real SFTP upload
}

bool SshExecutor::downloadFile(
    const std::string& remotePath,
    const std::string& localPath)
{
    (void)localPath;
    auto r = execute("cat '" + remotePath + "'");
    return static_cast<bool>(r); // TODO: write r.stdOut to localPath
}

bool SshExecutor::fileExists(const std::string& remotePath)
{
    auto r = execute("test -e '" + remotePath + "' && echo 1 || echo 0");
    return r && r.stdOut.find('1') != std::string::npos;
}

std::string SshExecutor::readFile(const std::string& remotePath)
{
    auto r = execute("cat '" + remotePath + "'");
    return r ? r.stdOut : std::string{};
}

bool SshExecutor::writeFile(
    const std::string& remotePath,
    const std::string& content)
{
    const std::string cmd =
        "cat > '" + remotePath + "' << 'SSHEOF'\n" + content + "\nSSHEOF";
    return static_cast<bool>(execute(cmd));
}

std::vector<std::string> SshExecutor::listDirectory(
    const std::string& remotePath)
{
    auto r = execute("ls -1 '" + remotePath + "'");
    if (!r) return {};

    std::vector<std::string> entries;
    std::istringstream       stream(r.stdOut);
    std::string              line;
    while (std::getline(stream, line))
        if (!line.empty()) entries.push_back(line);
    return entries;
}

// ── Private ───────────────────────────────────────────────────────────────────

bool SshExecutor::resolveAndConnect()
{
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const std::string portStr = std::to_string(config_.port);
    addrinfo*         res     = nullptr;

    if (getaddrinfo(config_.host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        Logging::Logger::error("[SshExecutor] DNS resolution failed for {}",
                               config_.host);
        return false;
    }

    SocketType sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == kInvalidSocket) {
        freeaddrinfo(res);
        Logging::Logger::error("[SshExecutor] socket() failed");
        return false;
    }

    if (::connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        freeaddrinfo(res);
        closesocket(sock);
        Logging::Logger::error("[SshExecutor] connect() failed to {}:{}",
                               config_.host, config_.port);
        return false;
    }

    freeaddrinfo(res);
    socket_ = static_cast<int>(sock);
    return true;
}

bool SshExecutor::authenticatePassword()
{
    const int rc = libssh2_userauth_password(
        session_,
        config_.username.c_str(),
        config_.password.c_str());

    if (rc != 0) {
        Logging::Logger::error("[SshExecutor] Password auth failed for {}@{}",
                               config_.username, config_.host);
        return false;
    }
    return true;
}

bool SshExecutor::authenticatePublicKey()
{
    const std::string pubKey = config_.publicKeyPath.empty()
        ? config_.privateKeyPath + ".pub"
        : config_.publicKeyPath;

    const int rc = libssh2_userauth_publickey_fromfile(
        session_,
        config_.username.c_str(),
        pubKey.c_str(),
        config_.privateKeyPath.c_str(),
        config_.passphrase.empty() ? nullptr : config_.passphrase.c_str());

    if (rc != 0) {
        Logging::Logger::error("[SshExecutor] Public-key auth failed for {}@{}",
                               config_.username, config_.host);
        return false;
    }
    return true;
}

ExecutorResult SshExecutor::runChannel(const std::string& command)
{
    LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(session_);
    if (!channel)
        return ExecutorResult::fail("Failed to open SSH channel");

    if (libssh2_channel_exec(channel, command.c_str()) != 0) {
        libssh2_channel_free(channel);
        return ExecutorResult::fail("Failed to exec: " + command);
    }

    std::string                      stdOut;
    std::string                      stdErr;
    std::array<char, kSshReadBuffer> buf{};
    ssize_t                          nread;

    while ((nread = libssh2_channel_read(channel, buf.data(), buf.size())) > 0)
        stdOut.append(buf.data(), static_cast<std::size_t>(nread));

    while ((nread = libssh2_channel_read_stderr(channel, buf.data(),
                                                 buf.size())) > 0)
        stdErr.append(buf.data(), static_cast<std::size_t>(nread));

    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    const int exitCode = libssh2_channel_get_exit_status(channel);
    libssh2_channel_free(channel);

    return ExecutorResult::fromProcess(exitCode, stdOut, stdErr);
}

void SshExecutor::cleanupSession()
{
    if (session_) {
        libssh2_session_disconnect(session_, "Normal shutdown");
        libssh2_session_free(session_);
        session_ = nullptr;
    }
    if (socket_ != static_cast<int>(kInvalidSocket)) {
        closesocket(static_cast<SocketType>(socket_));
        socket_ = static_cast<int>(kInvalidSocket);
    }
}

} // namespace OpenC3::Core::Connection
