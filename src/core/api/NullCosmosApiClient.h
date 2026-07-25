#pragma once

#include "ICosmosApiClient.h"

namespace OpenC3::Core::Api {

/// Null-object implementation of ICosmosApiClient.
///
/// Mirrors NullCommandExecutor's role for the shell transport: services can be
/// constructed and called before any COSMOS endpoint is configured, so no
/// service needs a null check or a "am I connected yet" branch. Every call
/// fails with a message the UI can show verbatim.
class NullCosmosApiClient final : public ICosmosApiClient {
public:
    void configure(const ApiConfig& /*config*/) override {}

    [[nodiscard]] const ApiConfig& config() const noexcept override { return config_; }

    [[nodiscard]] bool connect()    override { return false; }
    void               disconnect() override {}
    [[nodiscard]] bool isConnected() const noexcept override { return false; }

    [[nodiscard]] std::string lastError() const override { return kNotConnected; }

    [[nodiscard]] ApiResponse get(const std::string&, const QueryParams&) override
    {
        return ApiResponse::fail(kNotConnected);
    }

    [[nodiscard]] ApiResponse post(
        const std::string&, const std::string&, const QueryParams&) override
    {
        return ApiResponse::fail(kNotConnected);
    }

    [[nodiscard]] ApiResponse postForm(
        const std::string&, const QueryParams&, const QueryParams&) override
    {
        return ApiResponse::fail(kNotConnected);
    }

    [[nodiscard]] ApiResponse postFile(
        const std::string&, const std::string&, const std::string&,
        const QueryParams&, const QueryParams&) override
    {
        return ApiResponse::fail(kNotConnected);
    }

    [[nodiscard]] ApiResponse del(const std::string&, const QueryParams&) override
    {
        return ApiResponse::fail(kNotConnected);
    }

private:
    static constexpr const char* kNotConnected =
        "Not connected to a COSMOS API endpoint";

    ApiConfig config_;
};

} // namespace OpenC3::Core::Api
