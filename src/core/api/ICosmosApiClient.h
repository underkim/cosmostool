#pragma once

#include "ApiConfig.h"
#include "ApiResponse.h"

#include <string>
#include <utility>
#include <vector>

namespace OpenC3::Core::Api {

/// Ordered query-string parameters. A vector of pairs rather than a map so a
/// route that legitimately repeats a key stays expressible and the generated
/// URL is deterministic (useful when diffing logs).
using QueryParams = std::vector<std::pair<std::string, std::string>>;

/// The COSMOS HTTP API abstraction, and the second transport this application
/// speaks alongside ICommandExecutor.
///
/// Why this exists: a shell session on the COSMOS host can start/stop
/// containers and edit files, but it cannot answer any question about COSMOS
/// itself. Installed plugins live in MinIO + Redis (not on disk), targets and
/// packets are Redis-backed models, and installing a plugin is a two-phase
/// server-side operation. All of that is only reachable through the API that
/// traefik exposes on port 2900. Shell remains the right transport for Docker
/// and file editing; this is the right transport for COSMOS state.
///
/// Implementors: HttpCosmosApiClient (Qt Network), NullCosmosApiClient
/// Consumers: COSMOS-aware services, via dependency injection
///
/// Every method is synchronous, matching ICommandExecutor. Callers that need
/// async behaviour invoke from a worker thread (QtConcurrent::run in the
/// ViewModel layer, as the existing ViewModels already do for shell calls).
///
/// Implementations are responsible for attaching the configured scope to
/// every request - COSMOS's authorize() rejects a scope-less call outright -
/// so callers never pass scope by hand.
class ICosmosApiClient {
public:
    virtual ~ICosmosApiClient() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Apply endpoint configuration. Safe to call before or after connect();
    /// implementations must not carry credentials across a reconfigure.
    virtual void configure(const ApiConfig& config) = 0;

    [[nodiscard]] virtual const ApiConfig& config() const noexcept = 0;

    /// Verify the endpoint is reachable and the credentials are accepted.
    /// Implementations should probe a cheap authenticated route rather than
    /// merely opening a socket, so a wrong password fails here instead of
    /// surfacing later as a confusing 401 on the first real call.
    [[nodiscard]] virtual bool connect() = 0;

    virtual void disconnect() = 0;

    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// Why the most recent connect() or request failed. Empty on success.
    [[nodiscard]] virtual std::string lastError() const { return {}; }

    // ── Requests ──────────────────────────────────────────────────────────────

    /// `path` is relative to the API root and must begin with a slash,
    /// e.g. "/openc3-api/plugins". The scope parameter is added by the
    /// implementation.
    [[nodiscard]] virtual ApiResponse get(
        const std::string& path,
        const QueryParams& params = {}) = 0;

    /// POST with a JSON request body (Content-Type: application/json).
    /// Pass an empty body for the several COSMOS routes that take their
    /// arguments in the path or query string only.
    [[nodiscard]] virtual ApiResponse post(
        const std::string& path,
        const std::string& jsonBody = {},
        const QueryParams& params   = {}) = 0;

    /// POST as application/x-www-form-urlencoded. Needed because parts of the
    /// COSMOS API read their arguments from Rails params rather than a JSON
    /// body - notably plugins#install, which expects plugin_hash as a form
    /// field.
    [[nodiscard]] virtual ApiResponse postForm(
        const std::string& path,
        const QueryParams& formFields,
        const QueryParams& params = {}) = 0;

    /// POST multipart/form-data uploading a single local file.
    /// This is how a .gem reaches plugins#create, which reads params[:plugin]
    /// as an uploaded file and returns the plugin_hash that phase two needs.
    [[nodiscard]] virtual ApiResponse postFile(
        const std::string& path,
        const std::string& fieldName,
        const std::string& localFilePath,
        const QueryParams& extraFields = {},
        const QueryParams& params      = {}) = 0;

    [[nodiscard]] virtual ApiResponse del(
        const std::string& path,
        const QueryParams& params = {}) = 0;
};

} // namespace OpenC3::Core::Api
