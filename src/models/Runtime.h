#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

/// One target as reported by TargetModel (GET target_names / get_target
/// through the JSON-DRB API), not a locally-edited plugin source folder -
/// this only exists once the owning plugin is actually installed in COSMOS.
struct RuntimeTarget {
    std::string name;
};

/// One command or telemetry packet name under a target, from
/// get_all_cmd_names / get_all_tlm_names.
struct RuntimePacket {
    std::string targetName;
    std::string packetName;
};

/// One telemetry item's current value, as returned by get_tlm_packet /
/// get_tlm_values. `value` and `limitsState` are already stringified (JSON-
/// DRB returns arbitrary JSON types for `value`; this app only displays it,
/// never round-trips it back into a typed command parameter).
struct TelemetryItemValue {
    std::string itemName;
    std::string value;
    std::string limitsState;  ///< e.g. "GREEN", "YELLOW_LOW", "RED_HIGH", or empty
};

/// Outcome of a single JSON-DRB call (tlm/cmd/etc. through /openc3-api/api).
/// Distinguishes a transport/HTTP failure from a COSMOS-side JSON-RPC error
/// (unknown target, permission denied, hazardous command rejected, ...) so
/// callers can show the real reason rather than a generic "failed".
struct RuntimeCallResult {
    bool        success{false};
    std::string errorMessage;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

} // namespace OpenC3::Models
