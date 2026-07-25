#pragma once

#include <string>

namespace OpenC3::Core::Api {

/// How the client authenticates against the COSMOS API.
///
/// This release targets COSMOS 6.10.4 **Core**, where authentication is a
/// single shared password rather than a user directory: openc3/lib/openc3/
/// utilities/authentication.rb's OpenC3Authentication simply returns
/// ENV['OPENC3_API_PASSWORD'] verbatim as the token, and authorization.rb
/// verifies it with AuthModel.verify(token). Enterprise swaps in
/// OpenC3KeycloakAuthentication and a real Bearer JWT; the enum exists so
/// adding that later doesn't require touching every call site, but only
/// Password is implemented today.
enum class ApiAuthMode {
    Password,   ///< Core: Authorization header carries the raw API password
    Keycloak    ///< Enterprise: not implemented in this release
};

/// Configuration for one COSMOS API endpoint.
struct ApiConfig {
    /// Base URL of the COSMOS front door (traefik), *without* a trailing
    /// slash and without the /openc3-api prefix - e.g. "http://localhost:2900".
    std::string baseUrl{"http://localhost:2900"};

    ApiAuthMode authMode{ApiAuthMode::Password};

    /// Core's OPENC3_API_PASSWORD. Sent as the Authorization header value
    /// exactly as-is: Core does NOT use a "Bearer " prefix (see
    /// OpenC3Authentication#token, which ignores include_bearer entirely).
    /// SECURITY: persisted through the same protection path as the SSH
    /// password (see SettingsService); never log this value.
    std::string password;

    /// Every COSMOS API route requires a scope - authorize() raises
    /// "Scope is required" when it is missing - so it is part of the
    /// connection identity, not an optional per-call argument. "DEFAULT" is
    /// the scope Core always creates at install time.
    std::string scope{"DEFAULT"};

    int connectTimeoutMs{10'000};
    int requestTimeoutMs{30'000};

    /// Whether to accept self-signed / otherwise unverifiable TLS certs.
    /// Off by default; only meaningful for https base URLs.
    bool allowInsecureTls{false};
};

} // namespace OpenC3::Core::Api
