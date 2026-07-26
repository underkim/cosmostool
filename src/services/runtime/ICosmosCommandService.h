#pragma once

#include "models/Command.h"

#include <string>
#include <utility>
#include <vector>

namespace OpenC3::Services {

/// One command parameter as entered by the user: name plus its raw text
/// value. The service (not the UI) decides how to encode each value in the
/// JSON-DRB call - COSMOS's own `cmd('TGT CMD with PARAM value')` text
/// syntax treats a bare numeric-looking value as a number and everything
/// else as a string (see extract.rb#add_cmd_parameter), and this interface
/// follows the same convention when building the params hash for the
/// 3-argument cmd(target, command, params) call.
using CommandParameter = std::pair<std::string, std::string>;

/// COSMOS command *sending*, over the JSON-DRB API
/// (POST /openc3-api/api -> cmd/cmd_no_hazardous_check/...).
///
/// WHY THIS IS SEPARATE FROM ICosmosRuntimeService
/// ICosmosRuntimeService is deliberately read-only (see its header) - target/
/// command/telemetry *browsing* has no safety implications. Sending a
/// command can touch a live spacecraft or hardware interface, so it gets its
/// own interface and its own explicit hazardous-command confirmation flow
/// (see Models::CommandSendResult) rather than being bolted onto the
/// browser.
///
/// Every send first calls the safety-checked `cmd` method. If COSMOS
/// responds with a HazardousError (JSON-RPC error code -32502), the caller
/// must re-confirm with the user and then call sendConfirmed(), which uses
/// `cmd_no_hazardous_check` - mirroring exactly what OpenC3::Script's own
/// prompt_for_hazardous flow does, never silently downgrading the check
/// itself.
///
/// Implementations must remain safe to call before a connection exists; the
/// null API client turns every call into a clean failure.
class ICosmosCommandService {
public:
    virtual ~ICosmosCommandService() = default;

    /// Sends with full range and hazardous checks (COSMOS's `cmd`). Returns
    /// a hazardous-confirmation result rather than sending anything further
    /// when COSMOS raises HazardousError.
    [[nodiscard]] virtual Models::CommandSendResult send(
        const std::string&                    targetName,
        const std::string&                    commandName,
        const std::vector<CommandParameter>&  parameters) = 0;

    /// Sends with range checks but without the hazardous check (COSMOS's
    /// `cmd_no_hazardous_check`) - only ever call this after the user has
    /// explicitly confirmed a hazardousConfirmation() result from send().
    [[nodiscard]] virtual Models::CommandSendResult sendConfirmed(
        const std::string&                    targetName,
        const std::string&                    commandName,
        const std::vector<CommandParameter>&  parameters) = 0;

    /// True when the underlying API client currently has a live,
    /// authenticated endpoint.
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace OpenC3::Services
