#include "CosmosRuntimeService.h"

#include "core/api/CosmosRoutes.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>

namespace OpenC3::Services {

using Core::Api::ApiResponse;
using json = nlohmann::json;

namespace {

// COSMOS's JSON-DRB dispatch (json_drb.rb#process_request) always echoes
// back a JSON-RPC 2.0 envelope with either "result" or "error" - HTTP itself
// stays 200 in the success case and maps error codes to 4xx/5xx (see
// api_controller.rb#handle_post), so the JSON body is the source of truth
// either way.
struct DrbCallResult {
    bool    success{false};
    json    result;
    std::string errorMessage;
};

DrbCallResult callDrb(Core::Api::ICosmosApiClient& api, const std::string& method,
                      const json& params, int requestId)
{
    DrbCallResult out;

    // params is always an array in JSON-RPC; COSMOS API methods here take
    // positional args only (target_name, packet_name, ...) - no method used
    // by this service needs keyword_params beyond the scope/token the
    // controller injects itself from the Authorization header, so this
    // client never sends its own keyword_params for anything but scope.
    json request;
    request["jsonrpc"] = "2.0";
    request["method"]  = method;
    request["params"]  = params;
    request["id"]       = requestId;
    // json_rpc.rb#from_json does `hash['keyword_params']['token'] = ...`,
    // which raises NoMethodError on nil if keyword_params is absent from the
    // request body - it must always be present, even if only carrying scope.
    request["keyword_params"] = json::object({{"scope", api.config().scope}});

    auto response = api.post(Core::Api::Routes::kApiDrb, request.dump(), {});
    if (!response) {
        out.errorMessage = !response.errorMessage.empty()
            ? response.errorMessage
            : ("HTTP " + std::to_string(response.statusCode));
        // A JSON-RPC error still carries a real message in the body even
        // though the transport reports failure (4xx/5xx) - prefer it.
        if (!response.body.empty()) {
            try {
                const auto parsed = json::parse(response.body);
                if (parsed.is_object() && parsed.contains("error")
                    && parsed["error"].is_object()
                    && parsed["error"].contains("message"))
                {
                    out.errorMessage = parsed["error"]["message"].get<std::string>();
                }
            } catch (const json::exception&) {
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
            out.errorMessage = parsed["error"].is_object()
                                    && parsed["error"].contains("message")
                                    && parsed["error"]["message"].is_string()
                ? parsed["error"]["message"].get<std::string>()
                : "COSMOS returned a JSON-RPC error.";
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

} // namespace

CosmosRuntimeService::CosmosRuntimeService(Core::Api::ICosmosApiClient& api)
    : api_(api)
{}

bool CosmosRuntimeService::isAvailable() const
{
    return api_.isConnected();
}

std::vector<Models::RuntimeTarget> CosmosRuntimeService::listTargets()
{
    std::vector<Models::RuntimeTarget> targets;

    // get_target_names has no positional args of its own.
    const auto call = callDrb(api_, "get_target_names", json::array(),
                              nextRequestId_.fetch_add(1));
    if (!call.success) {
        Logging::Logger::warn("[Runtime] Listing targets failed: {}", call.errorMessage);
        return targets;
    }
    if (!call.result.is_array()) return targets;

    for (const auto& entry : call.result) {
        if (!entry.is_string()) continue;
        targets.push_back(Models::RuntimeTarget{entry.get<std::string>()});
    }
    return targets;
}

namespace {

std::vector<Models::RuntimePacket> listPacketsByMethod(
    Core::Api::ICosmosApiClient& api, std::atomic<int>& requestId,
    const std::string& method, const std::string& targetName)
{
    std::vector<Models::RuntimePacket> packets;
    if (targetName.empty()) return packets;

    const auto call = callDrb(api, method, json::array({targetName}),
                              requestId.fetch_add(1));
    if (!call.success) {
        Logging::Logger::warn("[Runtime] {} failed for {}: {}",
                              method, targetName, call.errorMessage);
        return packets;
    }
    if (!call.result.is_array()) return packets;

    // get_all_cmd_names/get_all_tlm_names both render a bare array of
    // packet-name strings.
    for (const auto& entry : call.result) {
        if (!entry.is_string()) continue;
        Models::RuntimePacket packet;
        packet.targetName = targetName;
        packet.packetName = entry.get<std::string>();
        packets.push_back(std::move(packet));
    }
    return packets;
}

} // namespace

std::vector<Models::RuntimePacket> CosmosRuntimeService::listCommands(
    const std::string& targetName)
{
    return listPacketsByMethod(api_, nextRequestId_, "get_all_cmd_names", targetName);
}

std::vector<Models::RuntimePacket> CosmosRuntimeService::listTelemetry(
    const std::string& targetName)
{
    return listPacketsByMethod(api_, nextRequestId_, "get_all_tlm_names", targetName);
}

std::vector<Models::TelemetryItemValue> CosmosRuntimeService::telemetryValues(
    const std::string& targetName, const std::string& packetName)
{
    std::vector<Models::TelemetryItemValue> values;
    if (targetName.empty() || packetName.empty()) return values;

    // get_tlm_packet(target, packet) -> array of [item_name, value, limits_state]
    // triples (see tlm_api.rb#get_tlm_packet).
    const auto call = callDrb(api_, "get_tlm_packet",
                              json::array({targetName, packetName}),
                              nextRequestId_.fetch_add(1));
    if (!call.success) {
        Logging::Logger::warn("[Runtime] get_tlm_packet failed for {} {}: {}",
                              targetName, packetName, call.errorMessage);
        return values;
    }
    if (!call.result.is_array()) return values;

    for (const auto& entry : call.result) {
        if (!entry.is_array() || entry.size() < 2) continue;
        Models::TelemetryItemValue value;
        value.itemName = entry[0].is_string() ? entry[0].get<std::string>()
                                               : entry[0].dump();
        value.value = entry[1].is_string() ? entry[1].get<std::string>()
                                            : entry[1].dump();
        if (entry.size() >= 3 && !entry[2].is_null()) {
            value.limitsState = entry[2].is_string() ? entry[2].get<std::string>()
                                                       : entry[2].dump();
        }
        values.push_back(std::move(value));
    }
    return values;
}

} // namespace OpenC3::Services
