#pragma once

#include <string>

namespace OpenC3::Core::Api {

/// Result returned by every ICosmosApiClient operation.
///
/// Deliberately mirrors Connection::ExecutorResult's value-or-error shape
/// (success flag + payload + errorMessage + explicit operator bool) so the
/// service layer handles an HTTP call and a shell call the same way and
/// neither transport needs exceptions.
struct ApiResponse {
    bool        success{false};
    int         statusCode{0};   ///< HTTP status; 0 when the request never left the client
    std::string body;            ///< Response body (JSON for every COSMOS API route we call)
    std::string errorMessage;

    // ── Factory helpers ───────────────────────────────────────────────────────
    [[nodiscard]] static ApiResponse ok(std::string responseBody = {}, int status = 200)
    {
        ApiResponse r;
        r.success    = true;
        r.statusCode = status;
        r.body       = std::move(responseBody);
        return r;
    }

    [[nodiscard]] static ApiResponse fail(std::string error, int status = 0)
    {
        ApiResponse r;
        r.success      = false;
        r.statusCode   = status;
        r.errorMessage = std::move(error);
        return r;
    }

    /// Builds a result from a completed HTTP exchange. 2xx is success; every
    /// other status keeps the body because the COSMOS API reports failures as
    /// {"status":"error","message":"..."} with a 4xx/5xx code, and that message
    /// is the only useful diagnostic the caller can show the user.
    [[nodiscard]] static ApiResponse fromHttp(int status, std::string responseBody)
    {
        ApiResponse r;
        r.statusCode = status;
        r.body       = std::move(responseBody);
        r.success    = (status >= 200 && status <= 299);
        if (!r.success)
            r.errorMessage = "HTTP " + std::to_string(status);
        return r;
    }

    explicit operator bool() const noexcept { return success; }
};

} // namespace OpenC3::Core::Api
