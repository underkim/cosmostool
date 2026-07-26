#include "CosmosScriptService.h"

#include "core/api/CosmosRoutes.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>

namespace OpenC3::Services {

using Core::Api::ApiResponse;
using Core::Api::QueryParams;
using json = nlohmann::json;

namespace {

// COSMOS reports failures as {"status":"error","message":"..."} alongside a
// 4xx/5xx - same convention CosmosPluginService relies on, so the message is
// the only actionable diagnostic worth surfacing over the bare HTTP status.
std::string describeFailure(const ApiResponse& response, const std::string& action)
{
    if (!response.body.empty()) {
        try {
            const auto parsed = json::parse(response.body);
            if (parsed.is_object() && parsed.contains("message")) {
                const auto& message = parsed["message"];
                if (message.is_string() && !message.get<std::string>().empty())
                    return action + " failed: " + message.get<std::string>();
            }
        } catch (const json::exception&) { // NOLINT(bugprone-empty-catch)
            // Not JSON (an HTML error page from traefik, say).
        }
    }
    if (!response.errorMessage.empty())
        return action + " failed: " + response.errorMessage;
    return action + " failed";
}

std::string stringField(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key)) return {};
    const auto& value = object[key];
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return value.dump();
    return {};
}

int intField(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key)) return 0;
    const auto& value = object[key];
    return value.is_number_integer() ? value.get<int>() : 0;
}

} // namespace

CosmosScriptService::CosmosScriptService(Core::Api::ICosmosApiClient& api)
    : api_(api)
{}

bool CosmosScriptService::isAvailable() const
{
    return api_.isConnected();
}

std::vector<Models::CosmosScript> CosmosScriptService::listScripts(
    const std::string& targetFilter)
{
    QueryParams params;
    if (!targetFilter.empty())
        params.emplace_back("target", targetFilter);

    auto response = api_.get(Core::Api::Routes::kScripts, params);

    std::vector<Models::CosmosScript> scripts;
    if (!response) {
        Logging::Logger::warn("[Script] {}",
                              describeFailure(response, "Listing scripts"));
        return scripts;
    }

    // scripts#index renders Script.all(scope, target), a bare JSON array of
    // bucket-relative path strings (trailing '*' marks a modified-but-not-
    // committed file - kept as-is since it is what run()/status() expect
    // back verbatim).
    try {
        const auto parsed = json::parse(response.body);
        if (!parsed.is_array()) return scripts;
        for (const auto& entry : parsed) {
            if (!entry.is_string()) continue;
            Models::CosmosScript script;
            script.path = entry.get<std::string>();
            scripts.push_back(std::move(script));
        }
    } catch (const json::exception& e) {
        Logging::Logger::warn("[Script] Could not parse script list: {}", e.what());
    }
    return scripts;
}

Models::ScriptRunResult CosmosScriptService::run(const std::string& scriptPath)
{
    if (scriptPath.empty())
        return Models::ScriptRunResult::fail("No script selected.");

    // /run/disconnect (not plain /run) is used deliberately: the connected
    // mode expects a live ActionCable channel for prompt()/breakpoint
    // round-trips that this desktop client does not implement, so a script
    // requiring interactive input would otherwise hang forever with no way
    // for the user to answer it. Disconnect mode runs the script straight
    // through, which is the same limitation this app's shell-based Run
    // Script already has.
    auto response = api_.post(
        Core::Api::Routes::scriptRunDisconnected(scriptPath), {}, {});

    if (!response)
        return Models::ScriptRunResult::fail(describeFailure(response, "Script run"));

    // scripts#run renders the numeric running_script_id as plain text
    // (render plain: running_script_id.to_s), not JSON - but tolerate a
    // JSON-encoded string/number too in case that ever changes upstream.
    std::string id = response.body;
    try {
        const auto parsed = json::parse(response.body);
        if (parsed.is_string()) id = parsed.get<std::string>();
        else if (parsed.is_number()) id = parsed.dump();
    } catch (const json::exception&) { // NOLINT(bugprone-empty-catch)
        // Plain text body - use as-is.
    }

    // Trim whitespace/newlines that may trail a plain-text render.
    const auto first = id.find_first_not_of(" \t\r\n");
    const auto last  = id.find_last_not_of(" \t\r\n");
    id = (first == std::string::npos) ? std::string{}
                                       : id.substr(first, last - first + 1);

    if (id.empty())
        return Models::ScriptRunResult::fail(
            "COSMOS accepted the run request but returned no running-script id.");

    Logging::Logger::info("[Script] Run started for {} (id {})", scriptPath, id);
    return Models::ScriptRunResult::ok(id);
}

Models::ScriptExecutionStatus CosmosScriptService::status(
    const std::string& runningScriptId)
{
    Models::ScriptExecutionStatus result;
    result.id = runningScriptId;
    if (runningScriptId.empty()) {
        result.errorMessage = "No running-script id to query.";
        return result;
    }

    auto response = api_.get(Core::Api::Routes::runningScript(runningScriptId), {});
    if (!response) {
        // running_script#show renders head :not_found (a 404 with no body)
        // once ScriptStatusModel has expired the entry entirely - that is
        // not a transport failure, just "nothing to report anymore".
        if (response.statusCode == 404) {
            result.querySucceeded = true;
            return result;
        }
        result.errorMessage = describeFailure(response, "Reading script status");
        return result;
    }
    result.querySucceeded = true;

    try {
        const auto parsed = json::parse(response.body);
        if (!parsed.is_object()) return result;

        result.found            = true;
        result.state            = stringField(parsed, "state");
        result.filename         = stringField(parsed, "filename");
        result.currentFilename  = stringField(parsed, "current_filename");
        result.lineNo           = intField(parsed, "line_no");

        // 'errors' (present once the script has hit a runtime error) is the
        // most actionable field to surface as output; fall back to nothing
        // rather than dumping the whole status hash at the user.
        if (parsed.contains("errors") && !parsed["errors"].is_null()) {
            const auto& errors = parsed["errors"];
            result.output = errors.is_string() ? errors.get<std::string>()
                                                : errors.dump();
        }
    } catch (const json::exception& e) {
        result.querySucceeded = false;
        result.errorMessage =
            std::string("Could not parse the COSMOS script status: ") + e.what();
    }
    return result;
}

bool CosmosScriptService::stop(const std::string& runningScriptId)
{
    if (runningScriptId.empty()) return false;
    auto response = api_.post(
        Core::Api::Routes::runningScriptStop(runningScriptId), {}, {});
    if (!response)
        Logging::Logger::warn("[Script] {}", describeFailure(response, "Script stop"));
    return static_cast<bool>(response);
}

bool CosmosScriptService::resume(const std::string& runningScriptId)
{
    if (runningScriptId.empty()) return false;
    auto response = api_.post(
        Core::Api::Routes::runningScriptGo(runningScriptId), {}, {});
    if (!response)
        Logging::Logger::warn("[Script] {}", describeFailure(response, "Script resume"));
    return static_cast<bool>(response);
}

bool CosmosScriptService::pause(const std::string& runningScriptId)
{
    if (runningScriptId.empty()) return false;
    auto response = api_.post(
        Core::Api::Routes::runningScriptPause(runningScriptId), {}, {});
    if (!response)
        Logging::Logger::warn("[Script] {}", describeFailure(response, "Script pause"));
    return static_cast<bool>(response);
}

} // namespace OpenC3::Services
