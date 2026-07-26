#include "CosmosPluginService.h"

#include "core/api/CosmosRoutes.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>

namespace OpenC3::Services {

using Core::Api::ApiResponse;
using json = nlohmann::json;

namespace {

/// COSMOS reports failures as {"status":"error","message":"..."} alongside a
/// 4xx/5xx. That message is the only actionable diagnostic, so prefer it over
/// the bare "HTTP 500" the transport produces.
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
        } catch (const json::exception&) {
            // Not JSON (an HTML error page from traefik, say) - fall through
            // to the transport-level message rather than surfacing raw markup.
        }
    }
    if (!response.errorMessage.empty())
        return action + " failed: " + response.errorMessage;
    return action + " failed";
}

/// Extracts a string field, tolerating null/missing/non-string, all of which
/// occur in PluginModel#as_json (title, description and friends are nil for a
/// plugin whose gemspec omitted them).
std::string stringField(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key)) return {};
    const auto& value = object[key];
    return value.is_string() ? value.get<std::string>() : std::string{};
}

/// COSMOS instance names carry an installation suffix so the same gem can be
/// installed more than once - "openc3-cosmos-demo-1.0.0.gem__20240101120000".
/// install_phase1 itself derives the gem name with `File.basename(path).split("__")[0]`,
/// so the same split is what recovers a human-facing name here.
std::string gemNameFromInstanceName(const std::string& instanceName)
{
    const auto separator = instanceName.find("__");
    return separator == std::string::npos ? instanceName
                                          : instanceName.substr(0, separator);
}

} // namespace

CosmosPluginService::CosmosPluginService(Core::Api::ICosmosApiClient& api)
    : api_(api)
{}

bool CosmosPluginService::isAvailable() const
{
    return api_.isConnected();
}

std::vector<Models::Plugin> CosmosPluginService::listInstalled()
{
    // GET /plugins (index) renders only `@model_class.names(...)` - a bare
    // array of strings. The 'all' member route is what returns the full
    // model hashes we need for version/description, so use that instead.
    auto response = api_.get(Core::Api::Routes::plugin("all"), {});

    std::vector<Models::Plugin> plugins;
    if (!response) {
        Logging::Logger::warn("[CosmosPlugin] {}",
                              describeFailure(response, "Listing plugins"));
        return plugins;
    }

    json parsed;
    try {
        parsed = json::parse(response.body);
    } catch (const json::exception& e) {
        Logging::Logger::warn("[CosmosPlugin] Could not parse plugin list: {}",
                              e.what());
        return plugins;
    }

    // plugins#show with id 'all' renders a hash keyed by plugin instance name.
    if (!parsed.is_object()) return plugins;

    for (const auto& [instanceName, entry] : parsed.items()) {
        Models::Plugin plugin;
        plugin.name   = instanceName;
        plugin.status = Models::PluginStatus::Installed;

        if (entry.is_object()) {
            plugin.description = stringField(entry, "description");
            if (plugin.description.empty())
                plugin.description = stringField(entry, "title");
        }

        // PluginModel carries no version field of its own - the version is
        // embedded in the gem filename that forms the instance name, so a
        // separate 'version' lookup would always come back empty.
        plugin.version = gemNameFromInstanceName(instanceName);

        plugins.push_back(std::move(plugin));
    }

    return plugins;
}

PluginOperationResult CosmosPluginService::uploadGem(const std::string& localGemPath)
{
    // plugins#create reads params[:plugin] as an uploaded file, so the
    // multipart field must be named exactly "plugin".
    auto response = api_.postFile(
        Core::Api::Routes::kPlugins, "plugin", localGemPath, {}, {});

    if (!response)
        return PluginOperationResult::fail(describeFailure(response, "Gem upload"));

    // Phase 1 renders the plugin hash itself (PluginModel#as_json), which
    // phase 2 must receive back unchanged. Validate that it is an object with
    // a name rather than assuming, so a surprising 200 doesn't get forwarded
    // into phase 2 as garbage.
    try {
        const auto parsed = json::parse(response.body);
        if (!parsed.is_object() || stringField(parsed, "name").empty())
            return PluginOperationResult::fail(
                "COSMOS accepted the upload but returned no plugin name; "
                "the file may not be a valid COSMOS plugin gem.");
    } catch (const json::exception& e) {
        return PluginOperationResult::fail(
            std::string("Could not parse the upload response from COSMOS: ") + e.what());
    }

    return PluginOperationResult::ok(response.body);
}

PluginOperationResult CosmosPluginService::installUploaded(
    const std::string& pluginId, const std::string& pluginHashJson)
{
    // plugins#install writes params[:plugin_hash] to a temp file and passes it
    // to `openc3cli load`. It is read from Rails params, not a JSON body, so
    // this must be form-encoded rather than posted as application/json.
    auto response = api_.postForm(
        Core::Api::Routes::pluginInstall(pluginId),
        {{"plugin_hash", pluginHashJson}},
        {});

    if (!response)
        return PluginOperationResult::fail(describeFailure(response, "Plugin install"));

    // The controller renders the spawned process name, so success here means
    // the install *job started* - not that it finished. The message says so
    // rather than letting the UI claim a completed install.
    std::string processName = response.body;
    try {
        const auto parsed = json::parse(response.body);
        if (parsed.is_string()) processName = parsed.get<std::string>();
    } catch (const json::exception&) {
        // Render of a bare string is still valid JSON; if it is not, keep the
        // raw body as the handle.
    }

    Logging::Logger::info("[CosmosPlugin] Install job started for {} ({})",
                          pluginId, processName);
    return PluginOperationResult::ok(
        "COSMOS started installing " + gemNameFromInstanceName(pluginId)
            + ". It may take a moment to appear.",
        processName);
}

PluginOperationResult CosmosPluginService::installGem(const std::string& localGemPath)
{
    auto uploaded = uploadGem(localGemPath);
    if (!uploaded) return uploaded;

    // The id for phase 2 is the gem name COSMOS returned in phase 1. On a real
    // 6.10.4 Core instance this is the *bare* gem filename (for example
    // "openc3-cosmos-demo-1.0.0.gem"); the installation suffix (__0, __1,
    // ...) is assigned by the asynchronous load job and only appears later in
    // listInstalled(). Phase 2 still requires the phase-1 name verbatim - do
    // not invent a suffix here. Uninstall, in contrast, must use the exact
    // suffixed instance id obtained from listInstalled().
    std::string pluginId;
    try {
        pluginId = stringField(json::parse(uploaded.message), "name");
    } catch (const json::exception& e) {
        return PluginOperationResult::fail(
            std::string("Could not read the plugin name from COSMOS: ") + e.what());
    }
    if (pluginId.empty())
        return PluginOperationResult::fail("COSMOS returned an empty plugin name.");

    return installUploaded(pluginId, uploaded.message);
}

PluginOperationResult CosmosPluginService::uninstall(const std::string& pluginId)
{
    auto response = api_.del(Core::Api::Routes::plugin(pluginId), {});

    if (!response)
        return PluginOperationResult::fail(
            describeFailure(response, "Plugin uninstall"));

    std::string processName = response.body;
    try {
        const auto parsed = json::parse(response.body);
        if (parsed.is_string()) processName = parsed.get<std::string>();
    } catch (const json::exception&) {
        // As above - keep the raw body when it is not a JSON string.
    }

    Logging::Logger::info("[CosmosPlugin] Uninstall job started for {} ({})",
                          pluginId, processName);
    return PluginOperationResult::ok(
        "COSMOS started removing " + gemNameFromInstanceName(pluginId) + ".",
        processName);
}

PluginJobStatus CosmosPluginService::jobStatus(const std::string& processName)
{
    PluginJobStatus result;
    result.processName = processName;
    if (processName.empty()) {
        result.errorMessage = "COSMOS returned an empty process name.";
        return result;
    }

    const auto response = api_.get(Core::Api::Routes::processStatus(processName), {});
    if (!response) {
        result.errorMessage = describeFailure(response, "Reading plugin job status");
        return result;
    }
    result.querySucceeded = true;

    try {
        const auto parsed = json::parse(response.body);
        const json* status = nullptr;

        if (parsed.is_object() && parsed.contains("state")) {
            status = &parsed;
        } else if (parsed.is_object()) {
            const auto it = parsed.find(processName);
            if (it != parsed.end() && it->is_object()) status = &(*it);
        } else if (parsed.is_array()) {
            for (const auto& entry : parsed) {
                if (entry.is_object()
                    && stringField(entry, "name") == processName)
                {
                    status = &entry;
                    break;
                }
            }
        }

        if (status == nullptr) return result;

        result.found  = true;
        result.state  = stringField(*status, "state");
        result.detail = stringField(*status, "detail");
        result.output = stringField(*status, "output");
        return result;
    } catch (const json::exception& e) {
        result.querySucceeded = false;
        result.errorMessage =
            std::string("Could not parse the COSMOS process status: ") + e.what();
        return result;
    }
}

} // namespace OpenC3::Services
