#include "CosmosCommandService.h"

#include "detail/JsonDrbCall.h"
#include "core/logging/Logger.h"

#include <cctype>
#include <stdexcept>

namespace OpenC3::Services {

using json = nlohmann::json;

namespace {

// COSMOS 6.10.4's extract.rb#add_cmd_parameter treats a bare numeric-looking
// text value as a number (Integer/Float) and everything else as a string
// when building cmd_params from `cmd('TGT CMD with PARAM value')` text.
// Following the same convention here means a value typed as "5" reaches
// COSMOS as the integer 5 rather than the string "5", matching what a user
// typing the equivalent text-form command would get - important because
// some parameter validation/range-checks differ for string vs numeric
// mismatches.
json convertParamValue(const std::string& raw)
{
    if (raw.empty()) return raw;

    // Integer (optionally signed, no interior whitespace).
    bool isInt = true;
    std::size_t start = (raw[0] == '+' || raw[0] == '-') ? 1 : 0;
    if (start >= raw.size()) isInt = false;
    for (std::size_t i = start; isInt && i < raw.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(raw[i]))) isInt = false;
    if (isInt) {
        try {
            return json(std::stoll(raw));
        } catch (const std::exception&) {
            // Falls through to float/string below (e.g. overflow).
        }
    }

    // Float (simple decimal form; scientific notation left as a string -
    // COSMOS's own is_float? is considerably more permissive, but this
    // client only needs to avoid mangling the common cases).
    bool isFloat = true;
    bool seenDot = false;
    std::size_t fstart = (raw[0] == '+' || raw[0] == '-') ? 1 : 0;
    if (fstart >= raw.size()) isFloat = false;
    for (std::size_t i = fstart; isFloat && i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '.') {
            if (seenDot) { isFloat = false; break; }
            seenDot = true;
        } else if (!std::isdigit(static_cast<unsigned char>(c))) {
            isFloat = false;
        }
    }
    if (isFloat && seenDot) {
        try {
            return json(std::stod(raw));
        } catch (const std::exception&) {
            // Fall through to string.
        }
    }

    return json(raw);
}

json buildParamsHash(const std::vector<CommandParameter>& parameters)
{
    json params = json::object();
    for (const auto& [name, value] : parameters)
        params[name] = convertParamValue(value);
    return params;
}

// Best-effort extraction of HazardousError#hazardous_description from the
// JSON-RPC error's "data" field, which (per Exception#as_json in
// top_level.rb) serializes a Ruby exception as
// {"class", "message", "backtrace", "instance_variables": {"@name" => val}}.
std::string extractHazardousDescription(const json& errorData)
{
    if (!errorData.is_object()) return {};
    const auto it = errorData.find("instance_variables");
    if (it == errorData.end() || !it->is_object()) return {};
    const auto descIt = it->find("@hazardous_description");
    if (descIt == it->end()) return {};
    return descIt->is_string() ? descIt->get<std::string>() : descIt->dump();
}

// True when the JSON-RPC error's "data" identifies a Ruby DisabledError -
// exact class-name match is far more robust than pattern-matching the
// English message text COSMOS happens to generate today.
bool isDisabledError(const json& errorData)
{
    if (!errorData.is_object()) return false;
    const auto it = errorData.find("class");
    return it != errorData.end() && it->is_string()
        && it->get<std::string>() == "DisabledError";
}

constexpr int kJsonRpcHazardousErrorCode = -32502;

} // namespace

CosmosCommandService::CosmosCommandService(Core::Api::ICosmosApiClient& api)
    : api_(api)
{}

bool CosmosCommandService::isAvailable() const
{
    return api_.isConnected();
}

Models::CommandSendResult CosmosCommandService::sendVia(
    const char* method, const std::string& targetName,
    const std::string& commandName, const std::vector<CommandParameter>& parameters)
{
    if (targetName.empty() || commandName.empty())
        return Models::CommandSendResult::fail("Target and command name are required.");

    // The 3-argument positional form - cmd(target, command, params) - avoids
    // re-parsing COSMOS's own text syntax on this side; params always
    // reaches COSMOS as a Hash even when empty.
    const auto call = detail::callJsonDrb(
        api_, method, json::array({targetName, commandName, buildParamsHash(parameters)}),
        nextRequestId_.fetch_add(1));

    if (call.success) {
        // _cmd_implementation returns the full command hash including
        // cmd_string, the human-readable "TGT CMD with ..." form.
        std::string cmdString;
        if (call.result.is_object() && call.result.contains("cmd_string")
            && call.result["cmd_string"].is_string())
            cmdString = call.result["cmd_string"].get<std::string>();
        Logging::Logger::info("[Command] Sent {} {} ({})",
                              targetName, commandName, cmdString);
        return Models::CommandSendResult::ok(std::move(cmdString));
    }

    if (call.errorCode == kJsonRpcHazardousErrorCode) {
        Logging::Logger::info("[Command] {} {} requires hazardous confirmation: {}",
                              targetName, commandName, call.errorMessage);
        return Models::CommandSendResult::hazardousConfirmation(
            extractHazardousDescription(call.errorData));
    }

    if (isDisabledError(call.errorData)) {
        Logging::Logger::warn("[Command] {} {} is disabled: {}",
                              targetName, commandName, call.errorMessage);
        return Models::CommandSendResult::disabledCommand(call.errorMessage);
    }

    Logging::Logger::warn("[Command] Sending {} {} failed: {}",
                          targetName, commandName, call.errorMessage);
    return Models::CommandSendResult::fail(call.errorMessage);
}

Models::CommandSendResult CosmosCommandService::send(
    const std::string& targetName, const std::string& commandName,
    const std::vector<CommandParameter>& parameters)
{
    return sendVia("cmd", targetName, commandName, parameters);
}

Models::CommandSendResult CosmosCommandService::sendConfirmed(
    const std::string& targetName, const std::string& commandName,
    const std::vector<CommandParameter>& parameters)
{
    return sendVia("cmd_no_hazardous_check", targetName, commandName, parameters);
}

} // namespace OpenC3::Services
