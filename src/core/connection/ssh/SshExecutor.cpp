#include "SshExecutor.h"
#include "core/logging/Logger.h"
#include "core/connection/ShellQuote.h"

#include <libssh2.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <array>
#include <cstdlib>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using SocketType = SOCKET;
   using SockLenType = int;
   constexpr SocketType kInvalidSocket = INVALID_SOCKET;
   inline bool setNonBlocking(SocketType s, bool nonBlocking) {
       u_long mode = nonBlocking ? 1 : 0;
       return ioctlsocket(s, FIONBIO, &mode) == 0;
   }
   inline bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cerrno>
   using SocketType = int;
   using SockLenType = socklen_t;
   constexpr SocketType kInvalidSocket = -1;
   inline void closesocket(int s) { close(s); }
   inline bool setNonBlocking(SocketType s, bool nonBlocking) {
       const int flags = fcntl(s, F_GETFL, 0);
       if (flags == -1) return false;
       return fcntl(s, F_SETFL, nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
   }
   inline bool wouldBlock() { return errno == EINPROGRESS; }
#endif

namespace OpenC3::Core::Connection {

namespace {
constexpr std::size_t kSshReadBuffer = 8192;

// Expands a leading "~" or "~/..." to the user's home directory. libssh2
// (and the raw fopen() it uses under the hood for key files) has no concept
// of "~" - a key path typed as "~/.ssh/id_rsa" (the exact placeholder shown
// in ConnectionConfig's own doc comment) would otherwise fail to open and
// surface as a generic auth failure with no hint why.
std::string expandHome(const std::string& path)
{
    if (path.empty() || path[0] != '~') return path;
    if (path.size() > 1 && path[1] != '/' && path[1] != '\\') return path; // "~user" form - not handled

#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (!home || !*home) return path;

    return std::string(home) + path.substr(1);
}

// Retrieves libssh2's own description of the last error on this session, if
// any - the difference between "wrong password" and "connection reset by
// peer" is otherwise lost the moment authenticate*() returns a plain bool.
std::string sessionErrorDetail(LIBSSH2_SESSION* session)
{
    if (!session) return {};
    char* msg    = nullptr;
    int   msgLen = 0;
    libssh2_session_last_error(session, &msg, &msgLen, 0);
    return (msg && msgLen > 0) ? std::string(msg, static_cast<std::size_t>(msgLen)) : std::string{};
}
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
    lastError_.clear();

    if (!resolveAndConnect()) return false;

    session_ = libssh2_session_init();
    if (!session_) {
        lastError_ = "Failed to initialize SSH session (libssh2_session_init failed)";
        Logging::Logger::error("[SshExecutor] libssh2_session_init failed");
        closesocket(static_cast<SocketType>(socket_));
        socket_ = static_cast<int>(kInvalidSocket);
        return false;
    }

    libssh2_session_set_blocking(session_, 1);

    if (libssh2_session_handshake(session_, socket_) != 0) {
        const std::string detail = sessionErrorDetail(session_);
        lastError_ = "SSH handshake failed with " + config_.host + ":" + std::to_string(config_.port)
            + (detail.empty() ? "" : " (" + detail + ")");
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
    // No SFTP subsystem is wired up yet, so the file is sent over the exec
    // channel: read it locally, base64-encode it (pure ASCII, so it cannot
    // contain shell metacharacters or prematurely close the heredoc below
    // regardless of the source file's content), and decode it back into
    // place on the remote side. Works for text and binary content alike.
    // Note: the whole encoded payload becomes one SSH exec command, so this
    // is appropriate for config-sized files; very large transfers still need
    // a real SFTP/SCP implementation.
    std::ifstream in(localPath, std::ios::binary);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string encoded = base64Encode(buf.str());

    // Vanishingly unlikely for base64 output, but fail closed like writeFile().
    if (contentEndsHeredoc(encoded, "SSHB64"))
        return false;

    const std::string cmd =
        "base64 -d > " + shellQuote(remotePath) + " << 'SSHB64'\n" + encoded + "SSHB64";
    return static_cast<bool>(execute(cmd));
}

bool SshExecutor::downloadFile(
    const std::string& remotePath,
    const std::string& localPath)
{
    auto r = execute("cat " + shellQuote(remotePath));
    if (!r) return false;

    std::ofstream out(localPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(r.stdOut.data(), static_cast<std::streamsize>(r.stdOut.size()));
    return static_cast<bool>(out);
}

bool SshExecutor::fileExists(const std::string& remotePath)
{
    auto r = execute("test -e " + shellQuote(remotePath) + " && echo 1 || echo 0");
    return r && r.stdOut.find('1') != std::string::npos;
}

std::string SshExecutor::readFile(const std::string& remotePath)
{
    auto r = execute("cat " + shellQuote(remotePath));
    return r ? r.stdOut : std::string{};
}

bool SshExecutor::writeFile(
    const std::string& remotePath,
    const std::string& content)
{
    // The quoted heredoc keeps $content literal, but a line equal to the
    // delimiter would end the heredoc early and let the rest run as commands.
    // Fail closed rather than risk a shell breakout.
    if (contentEndsHeredoc(content, "SSHEOF"))
        return false;

    const std::string cmd =
        "cat > " + shellQuote(remotePath) + " << 'SSHEOF'\n" + content + "\nSSHEOF";
    return static_cast<bool>(execute(cmd));
}

std::vector<std::string> SshExecutor::listDirectory(
    const std::string& remotePath)
{
    auto r = execute("ls -1 " + shellQuote(remotePath));
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
        lastError_ = "Could not resolve host '" + config_.host + "' - check the hostname/IP.";
        Logging::Logger::error("[SshExecutor] DNS resolution failed for {}",
                               config_.host);
        return false;
    }

    SocketType sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == kInvalidSocket) {
        freeaddrinfo(res);
        lastError_ = "Failed to create a network socket";
        Logging::Logger::error("[SshExecutor] socket() failed");
        return false;
    }

    // A blocking connect() to an unreachable/firewalled host (one that
    // silently drops packets rather than refusing the connection) can hang
    // for the platform's default TCP timeout - often well over a minute -
    // leaving the user staring at a "connecting..." spinner with no idea
    // whether it's still trying or already stuck. Connect non-blocking and
    // bound the wait ourselves to config_.connectTimeoutMs, so a bad
    // host/port fails predictably and quickly instead.
    const bool canTimeBox = setNonBlocking(sock, true);
    const int connectRc = ::connect(sock, res->ai_addr, static_cast<SockLenType>(res->ai_addrlen));

    bool connectOk = (connectRc == 0);
    if (!connectOk && canTimeBox && wouldBlock()) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);
        const int timeoutMs = config_.connectTimeoutMs > 0 ? config_.connectTimeoutMs : 10'000;
        timeval tv{};
        tv.tv_sec  = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        const int selectRc = ::select(static_cast<int>(sock) + 1, nullptr, &writeSet, nullptr, &tv);
        if (selectRc > 0) {
            int       sockErr    = 0;
            SockLenType sockErrLen = sizeof(sockErr);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                            reinterpret_cast<char*>(&sockErr), &sockErrLen) == 0 && sockErr == 0) {
                connectOk = true;
            }
        } else if (selectRc == 0) {
            freeaddrinfo(res);
            closesocket(sock);
            lastError_ = "Connection to " + config_.host + ":" + std::to_string(config_.port)
                + " timed out after " + std::to_string(timeoutMs / 1000)
                + "s - check the host/port, firewall, and VPN.";
            Logging::Logger::error("[SshExecutor] connect() timed out to {}:{}",
                                   config_.host, config_.port);
            return false;
        }
    }
    if (canTimeBox) setNonBlocking(sock, false);

    if (!connectOk) {
        freeaddrinfo(res);
        closesocket(sock);
        lastError_ = "Could not connect to " + config_.host + ":" + std::to_string(config_.port)
            + " - check the host/port and that the SSH server is reachable (firewall, VPN, etc.).";
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
        const std::string detail = sessionErrorDetail(session_);
        lastError_ = "Password authentication failed for " + config_.username + "@" + config_.host
            + (detail.empty() ? "" : " (" + detail + ")");
        Logging::Logger::error("[SshExecutor] Password auth failed for {}@{}",
                               config_.username, config_.host);
        return false;
    }
    return true;
}

bool SshExecutor::authenticatePublicKey()
{
    const std::string privateKey = expandHome(config_.privateKeyPath);
    const std::string pubKey = config_.publicKeyPath.empty()
        ? privateKey + ".pub"
        : expandHome(config_.publicKeyPath);

    // libssh2 (and the fopen() it does internally) surfaces a missing key
    // file as the same generic auth-failure return code as a wrong
    // passphrase or a rejected key - check for it up front so the error
    // message actually says what's wrong.
    if (std::ifstream(privateKey).fail()) {
        lastError_ = "Private key file not found: " + privateKey;
        Logging::Logger::error("[SshExecutor] Private key file not found: {}", privateKey);
        return false;
    }

    const int rc = libssh2_userauth_publickey_fromfile(
        session_,
        config_.username.c_str(),
        pubKey.c_str(),
        privateKey.c_str(),
        config_.passphrase.empty() ? nullptr : config_.passphrase.c_str());

    if (rc != 0) {
        const std::string detail = sessionErrorDetail(session_);
        lastError_ = "Public-key authentication failed for " + config_.username + "@" + config_.host
            + (detail.empty() ? "" : " (" + detail + ")");
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
