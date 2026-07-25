#pragma once

#include "models/Runtime.h"

#include <string>
#include <vector>

namespace OpenC3::Services {

/// Read-only COSMOS target/command/telemetry state, over the JSON-DRB API
/// (POST /openc3-api/api - the single endpoint behind which get_target_names,
/// get_all_cmd_names, get_all_tlm_names, tlm(), get_tlm_packet(), etc. all
/// live; see api_controller.rb#api -> OpenC3::Cts.instance.json_drb).
///
/// WHY READ-ONLY FOR NOW
/// Command *sending* (cmd/cmd_no_hazardous_check/...) touches a live
/// spacecraft or hardware interface and needs its own explicit safety UX
/// (confirmation, hazardous-command warnings, dry-run) before it belongs in
/// this client - see docs/developer-guide.md's roadmap. This interface only
/// answers "what does COSMOS say exists and what are the current values",
/// never sends anything.
///
/// Implementations must remain safe to call before a connection exists; the
/// null API client turns every call into a clean empty/failed result.
class ICosmosRuntimeService {
public:
    virtual ~ICosmosRuntimeService() = default;

    /// All target names in the connected scope (TargetModel.names, via
    /// get_target_names). Never Workspace source folders or Installed
    /// plugin instance names - a COSMOS logical target only exists once its
    /// owning plugin has been installed and its interface configured.
    [[nodiscard]] virtual std::vector<Models::RuntimeTarget> listTargets() = 0;

    /// Command packet names under one target (get_all_cmd_names).
    [[nodiscard]] virtual std::vector<Models::RuntimePacket> listCommands(
        const std::string& targetName) = 0;

    /// Telemetry packet names under one target (get_all_tlm_names).
    [[nodiscard]] virtual std::vector<Models::RuntimePacket> listTelemetry(
        const std::string& targetName) = 0;

    /// Current values for every visible (non-hidden) item in one telemetry
    /// packet (get_tlm_packet), each with its limits state.
    [[nodiscard]] virtual std::vector<Models::TelemetryItemValue> telemetryValues(
        const std::string& targetName, const std::string& packetName) = 0;

    /// True when the underlying API client currently has a live,
    /// authenticated endpoint.
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace OpenC3::Services
