#include "JsonDrbCall.h"

#include "core/api/CosmosRoutes.h"

namespace OpenC3::Services::detail {

using Core::Api::ApiResponse;
using json = nlohmann::json;

DrbCallResult callJsonDrb(
    Core::Api::ICosmosApiClient& api, const std::string& method,
    const json& params, int requestId)
{
    DrbCallResult out;

    // COSMOS's JSON-DRB dispatch (json_drb.rb#process_request) always
    // echoes back a JSON-RPC 2.0 envelope with either "result" or "error" -
    // HTTP itself stays 200 in the success case and maps error codes to
    // 4xx/5xx (see api_controller.rb#handle_post), so the JSON body is the
    // source of truth either way.
    json request;
    request["jsonrpc"] = "2.0";
    request["method"]  = method;
    request["params"]  = params;
    request["id"]      = requestId;
    // json_rpc.rb#from_json does `hash['keyword_params']['token'] = ...`,
    // which raises NoMethodError on nil if keyword_params is absent from the
    // request body - it must always be present, even if only carrying scope.
    request["keyword_params"] = json::object({{"scope", api.config().scope}});

    auto response = api.post(Core::Api::Routes::kApiDrb, request.dump(), {});
    if (!response) {
        out.errorMessage = !response.errorMessage.empty()
            ? response.errorMessage
            : ("HTTP " + std::to_string(response.statusCode));
        // A JSON-RPC error still carries a real message (and, for
        // Hazardous/CriticalCmd errors, the "data" payload the caller needs)
        // in the body even though the transport reports failure (4xx/5xx) -
        // prefer parsing it over the generic transport message.
        if (!response.body.empty()) {
            try {
                const auto parsed = json::parse(response.body);
                if (parsed.is_object() && parsed.contains("error")
                    && parsed["error"].is_object())
                {
                    const auto& error = parsed["error"];
                    if (error.contains("message") && error["message"].is_string())
                        out.errorMessage = error["message"].get<std::string>();
                    if (error.contains("code") && error["code"].is_number_integer())
                        out.errorCode = error["code"].get<int>();
                    if (error.contains("data"))
                        out.errorData = error["data"];
                }
            } catch (const json::exception&) { // NOLINT(bugprone-empty-catch)
                // Not JSON - keep the transport-level message.
            }
        }
        return out;
    }

    try {
        const auto parsed = json::parse(response.body);
        if (!parsed.is_object()) {
            out.errorMessage = "Malformed JSON-RPC response from COSMOS.";
            return out;
        }
        if (parsed.contains("error")) {
            const auto& error = parsed["error"];
            out.errorMessage = error.is_object()
                                    && error.contains("message")
                                    && error["message"].is_string()
                ? error["message"].get<std::string>()
                : "COSMOS returned a JSON-RPC error.";
            if (error.is_object() && error.contains("code")
                && error["code"].is_number_integer())
                out.errorCode = error["code"].get<int>();
            if (error.is_object() && error.contains("data"))
                out.errorData = error["data"];
            return out;
        }
        out.success = true;
        out.result  = parsed.value("result", json());
        return out;
    } catch (const json::exception& e) {
        out.errorMessage = std::string("Could not parse COSMOS API response: ") + e.what();
        return out;
    }
}

} // namespace OpenC3::Services::detail
