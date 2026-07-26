#include "CosmosRuntimeService.h"

#include "detail/JsonDrbCall.h"
#include "core/logging/Logger.h"

namespace OpenC3::Services {

using json = nlohmann::json;

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
    const auto call = detail::callJsonDrb(api_, "get_target_names", json::array(),
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

    const auto call = detail::callJsonDrb(api, method, json::array({targetName}),
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
    const auto call = detail::callJsonDrb(api_, "get_tlm_packet",
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
