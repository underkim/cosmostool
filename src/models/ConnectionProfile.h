#pragma once

#include "core/api/ApiConfig.h"
#include "core/connection/ConnectionConfig.h"

#include <string>

namespace OpenC3::Models {

using ConnectionMode = Core::Connection::ConnectionMode;
using AuthMethod     = Core::Connection::ConnectionConfig::AuthMethod;
using ApiAuthMode    = Core::Api::ApiAuthMode;

/// Persisted profile representing one connection target.
/// Stored in settings.json and presented in the Settings view.
struct ConnectionProfile {
    std::string    id;        // UUID, generated on creation
    std::string    name;      // Display name, e.g. "Dev Server"
    ConnectionMode mode{ConnectionMode::WSL};
    bool           isDefault{false};

    // ── COSMOS installation ───────────────────────────────────────────────────
    std::string cosmosRootPath{"/cosmos"};  // root of the COSMOS deployment on the target

    // ── WSL ───────────────────────────────────────────────────────────────────
    std::string wslDistribution{"Ubuntu"};

    // ── SSH ───────────────────────────────────────────────────────────────────
    std::string host;
    int         port{22};
    std::string username;
    AuthMethod  authMethod{AuthMethod::PublicKey};
    // SECURITY: SettingsService protects persisted password/passphrase values
    // before writing settings.json. Windows uses DPAPI; non-Windows builds use a
    // reversible compatibility obfuscation and log a warning because no OS
    // keychain dependency is required in this release.
    std::string password;
    std::string privateKeyPath;
    std::string publicKeyPath;
    std::string passphrase;

    int connectTimeoutMs{10'000};
    int commandTimeoutMs{30'000};

    // ── COSMOS API ────────────────────────────────────────────────────────────
    // Second transport, alongside the WSL/SSH shell above. The shell can drive
    // Docker and edit files, but COSMOS's own state - installed plugins,
    // targets, interfaces, microservices - lives in Redis/MinIO and is only
    // reachable through the HTTP API that traefik fronts on port 2900.
    //
    // apiBaseUrl is left empty by default so an upgraded profile keeps its
    // previous shell-only behaviour until the user opts in; resolvedApiBaseUrl()
    // supplies the conventional endpoint when it is blank.
    std::string apiBaseUrl;
    ApiAuthMode apiAuthMode{ApiAuthMode::Password};

    // SECURITY: this is OPENC3_API_PASSWORD from the COSMOS .env file. It is
    // persisted through the same protection path as the SSH password above
    // (DPAPI on Windows) and must never be logged.
    std::string apiPassword;

    // Every COSMOS API route requires a scope - authorize() rejects a request
    // without one - so it belongs to the connection identity rather than being
    // a per-call argument. Core always creates DEFAULT at install time.
    std::string apiScope{"DEFAULT"};

    bool apiAllowInsecureTls{false};
    int  apiRequestTimeoutMs{30'000};

    /// True when this profile has enough information to talk to the COSMOS API.
    /// Core requires a password: with $openc3_authorize enabled every route
    /// fails authorization without one, so a profile lacking it can only use
    /// the shell transport.
    [[nodiscard]] bool hasApiEndpoint() const
    {
        return !apiPassword.empty();
    }

    /// Falls back to the endpoint a stock COSMOS deployment exposes, so
    /// profiles created before the API existed - and users who never touch the
    /// field - still get a working default instead of an empty URL.
    [[nodiscard]] std::string resolvedApiBaseUrl() const
    {
        if (!apiBaseUrl.empty())
            return apiBaseUrl;
        if (mode == ConnectionMode::SSH && !host.empty())
            return "http://" + host + ":2900";
        return "http://localhost:2900";
    }

    [[nodiscard]] Core::Api::ApiConfig toApiConfig() const
    {
        Core::Api::ApiConfig cfg;
        cfg.baseUrl          = resolvedApiBaseUrl();
        cfg.authMode         = apiAuthMode;
        cfg.password         = apiPassword;
        cfg.scope            = apiScope.empty() ? std::string("DEFAULT") : apiScope;
        cfg.connectTimeoutMs = connectTimeoutMs;
        cfg.requestTimeoutMs = apiRequestTimeoutMs;
        cfg.allowInsecureTls = apiAllowInsecureTls;
        return cfg;
    }

    [[nodiscard]] Core::Connection::ConnectionConfig toConfig() const
    {
        Core::Connection::ConnectionConfig cfg;
        cfg.mode             = mode;
        cfg.cosmosRootPath   = cosmosRootPath;
        cfg.wslDistribution  = wslDistribution;
        cfg.host             = host;
        cfg.port             = port;
        cfg.username         = username;
        cfg.authMethod       = authMethod;
        cfg.password         = password;
        cfg.privateKeyPath   = privateKeyPath;
        cfg.publicKeyPath    = publicKeyPath;
        cfg.passphrase       = passphrase;
        cfg.connectTimeoutMs = connectTimeoutMs;
        cfg.commandTimeoutMs = commandTimeoutMs;
        return cfg;
    }
};

} // namespace OpenC3::Models
