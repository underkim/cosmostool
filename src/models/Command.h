#pragma once

#include <string>

namespace OpenC3::Models {

/// Outcome of a single cmd()/cmd_no_hazardous_check() JSON-DRB call.
///
/// COSMOS 6.10.4's Api#cmd raises HazardousError (not a normal failure) when
/// a command or one of its parameter values is marked hazardous - the
/// json_drb dispatcher turns that into a distinct JSON-RPC error code
/// (-32502, JsonRpcError::ErrorCode::HAZARDOUS_ERROR; see
/// openc3/lib/openc3/io/json_drb.rb#process_request and
/// openc3/lib/openc3/top_level.rb's HazardousError class). This struct keeps
/// that case distinct from a genuine failure so the UI can prompt for
/// confirmation and resend with cmd_no_hazardous_check rather than either
/// silently sending a hazardous command or treating the warning as an error.
struct CommandSendResult {
    bool        success{false};
    bool        hazardous{false};      ///< true: COSMOS refused pending confirmation
    bool        disabled{false};       ///< true: the command packet itself is DISABLED
    std::string hazardousDescription;  ///< HazardousError#hazardous_description, if present
    std::string cmdString;             ///< COSMOS's own formatted "TGT CMD with ..." on success
    std::string errorMessage;

    [[nodiscard]] static CommandSendResult ok(std::string formattedCommand)
    {
        CommandSendResult r;
        r.success   = true;
        r.cmdString = std::move(formattedCommand);
        return r;
    }

    [[nodiscard]] static CommandSendResult hazardousConfirmation(std::string description)
    {
        CommandSendResult r;
        r.hazardous           = true;
        r.hazardousDescription = std::move(description);
        return r;
    }

    [[nodiscard]] static CommandSendResult disabledCommand(std::string detail)
    {
        CommandSendResult r;
        r.disabled     = true;
        r.errorMessage = std::move(detail);
        return r;
    }

    [[nodiscard]] static CommandSendResult fail(std::string detail)
    {
        CommandSendResult r;
        r.errorMessage = std::move(detail);
        return r;
    }

    explicit operator bool() const noexcept { return success; }
};

} // namespace OpenC3::Models
