#pragma once

#include "core/api/ICosmosApiClient.h"

#include <nlohmann/json.hpp>

#include <string>

namespace OpenC3::Services::detail {

/// Result of one JSON-DRB call through POST /openc3-api/api. Internal
/// helper shared by CosmosRuntimeService (read-only browsing) and
/// CosmosCommandService (command sending) - both speak the exact same
/// JSON-RPC 2.0 envelope over the exact same endpoint (api_controller.rb#api
/// -> OpenC3::Cts.instance.json_drb.process_request), so the request/
/// response plumbing belongs in one place rather than being duplicated (and
/// risking drift) between them.
struct DrbCallResult {
    bool               success{false};
    nlohmann::json     result;
    /// JSON-RPC error code (see openc3/lib/openc3/io/json_rpc.rb's
    /// JsonRpcError::ErrorCode) when success is false and COSMOS returned a
    /// structured error, 0 otherwise. -32502 is HAZARDOUS_ERROR - the one
    /// code CosmosCommandService treats specially rather than as a generic
    /// failure.
    int                errorCode{0};
    /// Raw "data" field from the JSON-RPC error, when present. For a Ruby
    /// exception this is Exception#as_json's hash: {"class", "message",
    /// "backtrace", "instance_variables"} - HazardousError's
    /// hazardous_description lives at
    /// data["instance_variables"]["@hazardous_description"].
    nlohmann::json     errorData;
    std::string        errorMessage;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

/// Issues one JSON-RPC 2.0 call to COSMOS's JSON-DRB endpoint.
/// `params` is always a JSON array (positional args); `requestId` need only
/// be unique within one client's lifetime, matched back by COSMOS in the
/// response (id echoing - COSMOS never correlates on anything else, so a
/// wrong count here is harmless beyond making logs less traceable).
[[nodiscard]] DrbCallResult callJsonDrb(
    Core::Api::ICosmosApiClient& api,
    const std::string&           method,
    const nlohmann::json&        params,
    int                          requestId);

} // namespace OpenC3::Services::detail
