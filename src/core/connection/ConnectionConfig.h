#pragma once

#include <string>

namespace OpenC3::Core::Connection {

enum class ConnectionMode {
    WSL,
    SSH
};

/// Configuration passed to an executor at construction time.
struct ConnectionConfig {
    ConnectionMode mode{ConnectionMode::WSL};

    // ── WSL ───────────────────────────────────────────────────────────────────
    std::string wslDistribution{"Ubuntu"};  // e.g. "Ubuntu-22.04"

    // ── SSH ───────────────────────────────────────────────────────────────────
    std::string host;
    int         port{22};
    std::string username;

    enum class AuthMethod { Password, PublicKey };
    AuthMethod  authMethod{AuthMethod::PublicKey};

    std::string password;
    std::string privateKeyPath;    // ~/.ssh/id_rsa
    std::string publicKeyPath;     // ~/.ssh/id_rsa.pub (optional, derived if empty)
    std::string passphrase;        // key passphrase (if any)

    int connectTimeoutMs{10'000};
    int commandTimeoutMs{30'000};
};

} // namespace OpenC3::Core::Connection
