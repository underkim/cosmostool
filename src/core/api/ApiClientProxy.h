#pragma once

#include "ICosmosApiClient.h"
#include "NullCosmosApiClient.h"

#include <atomic>

namespace OpenC3::Core::Api {

/// Thread-safe API-client forwarder.
///
/// The API-transport counterpart to Connection::ExecutorProxy, and deliberately
/// identical in shape: services hold a stable ApiClientProxy& whose identity
/// never changes, while the proxy atomically delegates to whichever concrete
/// client is live. swap() is called by Application when a COSMOS endpoint is
/// connected; reset() on disconnect, error, or the start of a reconnect.
///
/// The reset-on-Connecting rule matters here for the same reason it does for
/// ExecutorProxy: resetting *before* the owning service replaces its client
/// instance guarantees the proxy is never left pointing at an object that is
/// about to be destroyed.
class ApiClientProxy final : public ICosmosApiClient {
public:
    ApiClientProxy();

    void swap(ICosmosApiClient* newClient) noexcept;
    void reset() noexcept;

    // ── ICosmosApiClient ──────────────────────────────────────────────────────
    void configure(const ApiConfig& config) override;
    [[nodiscard]] const ApiConfig& config() const noexcept override;

    [[nodiscard]] bool connect()    override;
    void               disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] std::string lastError() const override;

    [[nodiscard]] ApiResponse get(
        const std::string& path,
        const QueryParams& params) override;

    [[nodiscard]] ApiResponse post(
        const std::string& path,
        const std::string& jsonBody,
        const QueryParams& params) override;

    [[nodiscard]] ApiResponse postForm(
        const std::string& path,
        const QueryParams& formFields,
        const QueryParams& params) override;

    [[nodiscard]] ApiResponse postFile(
        const std::string& path,
        const std::string& fieldName,
        const std::string& localFilePath,
        const QueryParams& extraFields,
        const QueryParams& params) override;

    [[nodiscard]] ApiResponse del(
        const std::string& path,
        const QueryParams& params) override;

private:
    NullCosmosApiClient            nullClient_;
    std::atomic<ICosmosApiClient*> current_;
};

} // namespace OpenC3::Core::Api
