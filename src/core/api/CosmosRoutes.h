#pragma once

#include <string>

/// COSMOS 6.10.4 Core API routes.
///
/// Centralised so no route string is ever spelled out at a call site. Every
/// path here was taken from the actual 6.10.4 sources rather than from
/// documentation:
///   openc3-cosmos-cmd-tlm-api/config/routes.rb      (the "openc3-api" scope)
///   openc3-cosmos-script-runner-api/config/routes.rb ("script-api" scope)
///
/// Note the two different prefixes - the command/telemetry API is served under
/// /openc3-api and Script Runner under /script-api, both fronted by traefik on
/// the same port (2900 by default).
namespace OpenC3::Core::Api::Routes {

// ── Prefixes ──────────────────────────────────────────────────────────────────
inline constexpr const char* kApiPrefix    = "/openc3-api";
inline constexpr const char* kScriptPrefix = "/script-api";

// ── Health / status ───────────────────────────────────────────────────────────
inline constexpr const char* kPing           = "/openc3-api/ping";
inline constexpr const char* kInternalHealth = "/openc3-api/internal/health";
inline constexpr const char* kInternalStatus = "/openc3-api/internal/status";
inline constexpr const char* kInternalMetrics= "/openc3-api/internal/metrics";
inline constexpr const char* kTraefik        = "/openc3-api/traefik";

// ── Auth ──────────────────────────────────────────────────────────────────────
inline constexpr const char* kAuthTokenExists = "/openc3-api/auth/token-exists";
inline constexpr const char* kAuthVerify      = "/openc3-api/auth/verify";

// ── Scopes ────────────────────────────────────────────────────────────────────
inline constexpr const char* kScopes = "/openc3-api/scopes";

// ── Plugins ───────────────────────────────────────────────────────────────────
/// GET  list installed plugins / POST upload a .gem (phase 1 of install)
inline constexpr const char* kPlugins = "/openc3-api/plugins";

/// Phase 2 of install. Note the id goes *after* "install" in this route -
/// `post '/plugins/install/:id'` - which is the reverse of the usual Rails
/// member-route ordering and easy to get wrong.
[[nodiscard]] inline std::string pluginInstall(const std::string& id)
{
    return std::string("/openc3-api/plugins/install/") + id;
}

[[nodiscard]] inline std::string plugin(const std::string& id)
{
    return std::string("/openc3-api/plugins/") + id;
}

// ── Targets / interfaces / routers / microservices ────────────────────────────
inline constexpr const char* kTargets       = "/openc3-api/targets";
inline constexpr const char* kInterfaces    = "/openc3-api/interfaces";
inline constexpr const char* kRouters       = "/openc3-api/routers";
inline constexpr const char* kMicroservices = "/openc3-api/microservices";
inline constexpr const char* kProcessStatus = "/openc3-api/process_status";

/// GET a single ProcessStatusModel by the process name returned from plugin
/// install/uninstall. COSMOS 6.10.4 routes this as process_status#show.
[[nodiscard]] inline std::string processStatus(const std::string& name)
{
    return std::string("/openc3-api/process_status/") + name;
}

[[nodiscard]] inline std::string target(const std::string& name)
{
    return std::string("/openc3-api/targets/") + name;
}

[[nodiscard]] inline std::string interfaceByName(const std::string& name)
{
    return std::string("/openc3-api/interfaces/") + name;
}

[[nodiscard]] inline std::string microservice(const std::string& name)
{
    return std::string("/openc3-api/microservices/") + name;
}

[[nodiscard]] inline std::string microserviceStatus(const std::string& name)
{
    return std::string("/openc3-api/microservice_status/") + name;
}

// ── Storage ───────────────────────────────────────────────────────────────────
inline constexpr const char* kStorageBuckets = "/openc3-api/storage/buckets";
inline constexpr const char* kStorageVolumes = "/openc3-api/storage/volumes";

// ── JSON-DRB ──────────────────────────────────────────────────────────────────
/// The single POST endpoint behind which the entire COSMOS scripting API
/// lives (cmd, tlm, get_all_target_names, limits, ...). Requests are
/// JSON-RPC style and dispatched by api_controller.rb to json_drb.
inline constexpr const char* kApiDrb = "/openc3-api/api";

// ── Script Runner ─────────────────────────────────────────────────────────────
inline constexpr const char* kScriptPing       = "/script-api/ping";
inline constexpr const char* kScripts          = "/script-api/scripts";
inline constexpr const char* kRunningScript    = "/script-api/running-script";
inline constexpr const char* kCompletedScripts = "/script-api/completed-scripts";

[[nodiscard]] inline std::string scriptRun(const std::string& name)
{
    return std::string("/script-api/scripts/") + name + "/run";
}

[[nodiscard]] inline std::string runningScript(const std::string& id)
{
    return std::string("/script-api/running-script/") + id;
}

// ── Script Runner: script bodies and lifecycle actions ─────────────────────────
// routes.rb (script-api scope):
//   get  "/scripts" => "scripts#index"
//   post "/scripts/*name/run(/:disconnect)" => "scripts#run"
//   post "/running-script/:id/stop"   => "running_script#stop"
//   post "/running-script/:id/pause"  => "running_script#pause"
//   post "/running-script/:id/go"     => "running_script#go"
// `name` is a bucket-relative script path (e.g. "INST/procedures/foo.rb"),
// never a local filesystem path - it must be exactly what scripts#index
// returned, wildcard-matched by Rails' `*name` segment.

[[nodiscard]] inline std::string scriptRunDisconnected(const std::string& name)
{
    return std::string("/script-api/scripts/") + name + "/run/disconnect";
}

[[nodiscard]] inline std::string runningScriptStop(const std::string& id)
{
    return std::string("/script-api/running-script/") + id + "/stop";
}

[[nodiscard]] inline std::string runningScriptPause(const std::string& id)
{
    return std::string("/script-api/running-script/") + id + "/pause";
}

[[nodiscard]] inline std::string runningScriptGo(const std::string& id)
{
    return std::string("/script-api/running-script/") + id + "/go";
}

} // namespace OpenC3::Core::Api::Routes
