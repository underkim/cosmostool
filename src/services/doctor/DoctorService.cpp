#include "DoctorService.h"
#include "core/api/CosmosRoutes.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"
#include "services/runtime/detail/JsonDrbCall.h"

#include <nlohmann/json.hpp>

#include <sstream>

namespace OpenC3::Services {

using Models::HealthCheckResult;
using Models::HealthStatus;
using Models::DoctorReport;
using Core::Connection::shellQuote;
using json = nlohmann::json;

namespace {

// Command output embedded verbatim into a HealthCheckResult::detail carries
// the shell's trailing newline, which then shows up as a stray line break in
// the Doctor table's Detail column - trim it before display.
std::string trimTrailingNewline(std::string s)
{
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

/// The services COSMOS 6.10.4's compose.yaml defines. Matching is by
/// substring, because Compose prefixes the project name and appends an
/// instance index ("cosmos-openc3-redis-1"), and the project name is
/// configurable.
struct ExpectedService {
    const char* fragment;
    const char* label;
    bool        required;   ///< false for one-shot/optional containers
};

constexpr ExpectedService kExpectedServices[] = {
    {"openc3-traefik",                 "traefik (routing)",      true},
    {"openc3-redis-ephemeral",         "redis-ephemeral",        true},
    {"openc3-redis",                   "redis",                  true},
    {"openc3-minio",                   "minio (storage)",        true},
    {"openc3-operator",                "operator",               true},
    {"openc3-cosmos-cmd-tlm-api",      "cmd-tlm-api",            true},
    {"openc3-cosmos-script-runner-api","script-runner-api",      true},
    // init seeds the system then exits, so it is normally absent on a
    // healthy running stack - its absence must never be reported as a fault.
    {"openc3-cosmos-init",             "init (one-shot)",        false},
};

/// Extracts "message" from a COSMOS error body, which is the only actionable
/// text in a failed API response.
std::string apiMessage(const std::string& body)
{
    try {
        const auto parsed = json::parse(body);
        if (parsed.is_object() && parsed.contains("message")
            && parsed["message"].is_string())
            return parsed["message"].get<std::string>();
    } catch (const json::exception&) {
        // Not JSON (e.g. an HTML error page from traefik).
    }
    return {};
}

} // namespace

DoctorService::DoctorService(
    Core::Connection::ICommandExecutor& executor,
    Core::Api::ICosmosApiClient&        api,
    std::function<std::string()>        cosmosRootProvider)
    : executor_(executor)
    , api_(api)
    , cosmosRootProvider_(std::move(cosmosRootProvider))
{
    if (!cosmosRootProvider_)
        cosmosRootProvider_ = [] { return std::string("/cosmos"); };
    registerChecks();
}

void DoctorService::registerChecks()
{
    auto add = [&](std::string id, CheckFn fn) {
        checks_.emplace_back(id, fn);
        checkMap_[std::move(id)] = std::move(fn);
    };

    // L1 - infrastructure
    add("docker_installed",   [this]{ return checkDockerInstalled();   });
    add("docker_running",     [this]{ return checkDockerRunning();     });
    add("docker_compose",     [this]{ return checkDockerCompose();     });
    add("cpu_usage",          [this]{ return checkCpuUsage();          });
    add("memory_usage",       [this]{ return checkMemoryUsage();       });
    add("disk_space",         [this]{ return checkDiskSpace();         });
    add("network",            [this]{ return checkNetworkConnectivity();});
    add("time_sync",          [this]{ return checkTimeSynchronization();});

    // L2 - COSMOS stack
    add("cosmos_containers",  [this]{ return checkCosmosContainers();  });
    add("redis",              [this]{ return checkRedis();             });
    add("redis_ephemeral",    [this]{ return checkRedisEphemeral();    });
    add("minio",              [this]{ return checkMinio();             });
    add("openc3_version",     [this]{ return checkOpenC3Version();     });

    // L3 - COSMOS logic
    add("api_reachable",      [this]{ return checkApiReachable();      });
    add("scope",              [this]{ return checkScope();             });
    add("plugins",            [this]{ return checkPlugins();           });
    add("microservices",      [this]{ return checkMicroservices();     });
    add("interfaces",         [this]{ return checkInterfaces();        });
}

DoctorReport DoctorService::runAll(
    std::function<void(const HealthCheckResult&)> onProgress)
{
    DoctorReport report;
    for (auto& [id, fn] : checks_) {
        (void)id;
        auto r = fn();
        if (onProgress) onProgress(r);
        report.results.push_back(std::move(r));
    }
    return report;
}

HealthCheckResult DoctorService::runCheck(const std::string& checkId)
{
    auto it = checkMap_.find(checkId);
    if (it == checkMap_.end()) {
        HealthCheckResult r;
        r.checkId = checkId;
        r.name    = checkId;
        r.status  = HealthStatus::Fail;
        r.detail  = "Unknown check id";
        return r;
    }
    return it->second();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::vector<std::string> DoctorService::runningContainers()
{
    // Deliberately unfiltered. The previous implementation used
    // `--filter name=cosmos`, which silently excluded redis, minio, traefik
    // and operator - the containers whose failure actually breaks COSMOS.
    auto res = executor_.execute("docker ps --format '{{.Names}}' 2>/dev/null");
    std::vector<std::string> names;
    if (!res) return names;

    std::istringstream stream(res.stdOut);
    std::string        line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) names.push_back(line);
    }
    return names;
}

std::string DoctorService::findContainer(const std::string& fragment)
{
    for (const auto& name : runningContainers())
        if (name.find(fragment) != std::string::npos)
            return name;
    return {};
}

HealthCheckResult DoctorService::apiUnavailable(
    std::string checkId, std::string name)
{
    HealthCheckResult r;
    r.checkId  = std::move(checkId);
    r.name     = std::move(name);
    r.category = "COSMOS";
    // Skipped, not Fail: a profile with no API password is a legitimate
    // shell-only setup, and reporting it as broken would train users to
    // ignore Doctor output.
    r.status     = HealthStatus::Skipped;
    r.detail     = "COSMOS API not connected";
    r.suggestion = "Set an API password on the connection profile to enable "
                   "COSMOS status checks.";
    return r;
}

// ── L1: infrastructure ────────────────────────────────────────────────────────

HealthCheckResult DoctorService::checkDockerInstalled()
{
    HealthCheckResult r;
    r.checkId  = "docker_installed";
    r.name     = "Docker installed";
    r.category = "Docker";

    auto res = executor_.execute("which docker 2>/dev/null || command -v docker");
    if (res && !res.stdOut.empty()) {
        r.status = HealthStatus::Pass;
        r.detail = "docker found at " + trimTrailingNewline(res.stdOut);
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = "docker executable not found in PATH";
        r.suggestion = "Install Docker Engine: https://docs.docker.com/engine/install/";
    }
    return r;
}

HealthCheckResult DoctorService::checkDockerRunning()
{
    HealthCheckResult r;
    r.checkId  = "docker_running";
    r.name     = "Docker daemon running";
    r.category = "Docker";

    auto res = executor_.execute("docker info > /dev/null 2>&1 && echo OK");
    if (res && res.stdOut.find("OK") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = "Docker daemon is responding";
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = "Docker daemon is not running";
        r.suggestion = "Run: sudo systemctl start docker";
    }
    return r;
}

HealthCheckResult DoctorService::checkDockerCompose()
{
    HealthCheckResult r;
    r.checkId  = "docker_compose";
    r.name     = "Docker Compose available";
    r.category = "Docker";

    auto res = executor_.execute("docker compose version 2>/dev/null");
    if (res && res.stdOut.find("Docker Compose") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = trimTrailingNewline(res.stdOut);
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = "docker compose plugin not found";
        r.suggestion = "Install Docker Compose plugin for your distro";
    }
    return r;
}

HealthCheckResult DoctorService::checkCpuUsage()
{
    HealthCheckResult r;
    r.checkId  = "cpu_usage";
    r.name     = "CPU usage";
    r.category = "System";

    auto res = executor_.execute(
        "top -bn1 | grep 'Cpu(s)' | awk '{print $2+$4}'");
    if (!res) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not read CPU stats";
        return r;
    }
    double cpu = 0.0;
    try {
        cpu = std::stod(res.stdOut);
    } catch (...) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not parse CPU stats output";
        return r;
    }
    r.detail = std::to_string(static_cast<int>(cpu)) + "% in use";

    if (cpu < 80.0)      r.status = HealthStatus::Pass;
    else if (cpu < 95.0) r.status = HealthStatus::Warning;
    else {
        r.status     = HealthStatus::Fail;
        r.suggestion = "High CPU usage may impact OpenC3 performance";
    }
    return r;
}

HealthCheckResult DoctorService::checkMemoryUsage()
{
    HealthCheckResult r;
    r.checkId  = "memory_usage";
    r.name     = "Memory usage";
    r.category = "System";

    auto res = executor_.execute(
        "free -m | awk '/^Mem:/{printf \"%.1f\", $3/$2*100}'");
    if (!res) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not read memory stats";
        return r;
    }
    double pct = 0.0;
    try {
        pct = std::stod(res.stdOut);
    } catch (...) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not parse memory stats output";
        return r;
    }
    r.detail = std::to_string(static_cast<int>(pct)) + "% used";

    if (pct < 80.0)      r.status = HealthStatus::Pass;
    else if (pct < 90.0) r.status = HealthStatus::Warning;
    else {
        r.status     = HealthStatus::Fail;
        r.suggestion = "Consider increasing system RAM or stopping unused services";
    }
    return r;
}

HealthCheckResult DoctorService::checkDiskSpace()
{
    HealthCheckResult r;
    r.checkId  = "disk_space";
    r.name     = "Disk space";
    r.category = "System";

    auto res = executor_.execute(
        "df / | awk 'NR==2{gsub(\"%\",\"\",$5); print $5}'");
    if (!res) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not read disk stats";
        return r;
    }
    int pct = 0;
    try {
        pct = std::stoi(res.stdOut);
    } catch (...) {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not parse disk stats output";
        return r;
    }
    r.detail = std::to_string(pct) + "% used";

    if (pct < 80)      r.status = HealthStatus::Pass;
    else if (pct < 90) r.status = HealthStatus::Warning;
    else {
        r.status     = HealthStatus::Fail;
        r.suggestion = "Free disk space. OpenC3 logs and images can fill quickly.";
    }
    return r;
}

HealthCheckResult DoctorService::checkNetworkConnectivity()
{
    HealthCheckResult r;
    r.checkId  = "network";
    r.name     = "Network connectivity";
    r.category = "Network";

    auto res = executor_.execute("ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1 && echo OK");
    if (res && res.stdOut.find("OK") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = "Internet connectivity confirmed";
    } else {
        r.status     = HealthStatus::Warning;
        r.detail     = "Cannot reach 8.8.8.8";
        r.suggestion = "Check network configuration. Internet required for pulling images.";
    }
    return r;
}

HealthCheckResult DoctorService::checkTimeSynchronization()
{
    HealthCheckResult r;
    r.checkId  = "time_sync";
    r.name     = "Time synchronization";
    r.category = "System";

    auto res = executor_.execute(
        "timedatectl show --property=NTPSynchronized --value 2>/dev/null || echo unknown");
    if (res && res.stdOut.find("yes") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = "NTP synchronized";
    } else if (res && res.stdOut.find("no") != std::string::npos) {
        r.status     = HealthStatus::Warning;
        r.detail     = "NTP not synchronized";
        r.suggestion = "Run: sudo timedatectl set-ntp true";
    } else {
        r.status = HealthStatus::Skipped;
        r.detail = "Could not determine NTP status";
    }
    return r;
}

// ── L2: COSMOS stack ──────────────────────────────────────────────────────────

HealthCheckResult DoctorService::checkCosmosContainers()
{
    HealthCheckResult r;
    r.checkId  = "cosmos_containers";
    r.name     = "COSMOS containers";
    r.category = "COSMOS";

    const auto names = runningContainers();
    if (names.empty()) {
        r.status     = HealthStatus::Fail;
        r.detail     = "No running containers found";
        r.suggestion = "Start COSMOS with: ./openc3.sh start";
        return r;
    }

    // Reports exactly which services are missing rather than comparing a
    // count against an arbitrary threshold - "3 of 7" gives the user nothing
    // to act on, "operator, minio missing" does.
    std::vector<std::string> missing;
    for (const auto& service : kExpectedServices) {
        if (!service.required) continue;
        bool found = false;
        for (const auto& name : names) {
            if (name.find(service.fragment) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) missing.emplace_back(service.label);
    }

    if (missing.empty()) {
        r.status = HealthStatus::Pass;
        r.detail = "All expected COSMOS services are running";
        return r;
    }

    std::string joined;
    for (std::size_t i = 0; i < missing.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += missing[i];
    }
    r.status     = HealthStatus::Fail;
    r.detail     = "Not running: " + joined;
    r.suggestion = "Start the full stack with: ./openc3.sh start";
    return r;
}

HealthCheckResult DoctorService::checkRedis()
{
    HealthCheckResult r;
    r.checkId  = "redis";
    r.name     = "Redis (configuration store)";
    r.category = "COSMOS";

    std::string container;
    for (const auto& name : runningContainers()) {
        // "openc3-redis" is also a substring of
        // "openc3-redis-ephemeral". Accepting the first substring match can
        // make both Redis checks ping the ephemeral instance and report the
        // persistent configuration store healthy while it is actually down.
        if (name.find("openc3-redis") != std::string::npos
            && name.find("openc3-redis-ephemeral") == std::string::npos) {
            container = name;
            break;
        }
    }
    if (container.empty()) {
        r.status     = HealthStatus::Fail;
        r.detail     = "Redis container is not running";
        r.suggestion = "COSMOS stores all target/plugin configuration in Redis; "
                       "start the stack with ./openc3.sh start";
        return r;
    }

    // COSMOS 6.10.4 disables Redis's default ACL user and provides a
    // restricted nopass `healthcheck` user. An explicit empty password in
    // the URI makes redis-cli issue AUTH without reading or exposing the
    // real Redis credential.
    auto res = executor_.execute(
        "docker exec " + shellQuote(container)
        + " redis-cli --no-auth-warning -u 'redis://healthcheck:@127.0.0.1:6379'"
          " ping 2>&1");
    if (res && res.stdOut.find("PONG") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = container + " responded to PING";
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = container + " did not respond to PING";
        r.suggestion = "Redis is up but not answering; check its logs with "
                       "docker logs " + container;
    }
    return r;
}

HealthCheckResult DoctorService::checkRedisEphemeral()
{
    HealthCheckResult r;
    r.checkId  = "redis_ephemeral";
    r.name     = "Redis ephemeral (telemetry streams)";
    r.category = "COSMOS";

    // Distinct from the persistent instance: this one carries the live
    // command/telemetry streams, so losing it breaks realtime data while
    // configuration still looks healthy.
    const std::string container = findContainer("openc3-redis-ephemeral");
    if (container.empty()) {
        r.status     = HealthStatus::Fail;
        r.detail     = "Ephemeral Redis container is not running";
        r.suggestion = "Realtime telemetry streams flow through this instance; "
                       "start the stack with ./openc3.sh start";
        return r;
    }

    // The ephemeral instance listens on 6380 (not Redis's default 6379) and
    // exposes the same restricted healthcheck ACL user.
    auto res = executor_.execute(
        "docker exec " + shellQuote(container)
        + " redis-cli --no-auth-warning -u 'redis://healthcheck:@127.0.0.1:6380'"
          " ping 2>&1");
    if (res && res.stdOut.find("PONG") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = container + " responded to PING";
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = container + " did not respond to PING";
        r.suggestion = "Check its logs with docker logs " + container;
    }
    return r;
}

HealthCheckResult DoctorService::checkMinio()
{
    HealthCheckResult r;
    r.checkId  = "minio";
    r.name     = "MinIO (bucket storage)";
    r.category = "COSMOS";

    const std::string container = findContainer("openc3-minio");
    if (container.empty()) {
        r.status     = HealthStatus::Fail;
        r.detail     = "MinIO container is not running";
        r.suggestion = "Installed plugin gems and tool files live in MinIO "
                       "buckets; start the stack with ./openc3.sh start";
        return r;
    }

    r.status = HealthStatus::Pass;
    r.detail = container + " is running";
    return r;
}

HealthCheckResult DoctorService::checkOpenC3Version()
{
    HealthCheckResult r;
    r.checkId  = "openc3_version";
    r.name     = "OpenC3 version";
    r.category = "COSMOS";

    const std::string versionPath =
        cosmosRootProvider_() +
        "/openc3-cosmos-init/plugins/openc3-tool-base/VERSION";
    auto res = executor_.execute(
        "cat " + shellQuote(versionPath) + " 2>/dev/null || echo unknown");
    const std::string version = trimTrailingNewline(res.stdOut);

    if (res && version != "unknown" && !version.empty()) {
        r.status = HealthStatus::Pass;
        r.detail = "OpenC3 version: " + version;
    } else {
        r.status     = HealthStatus::Warning;
        r.detail     = "Could not determine OpenC3 version";
        r.suggestion = "Ensure OpenC3 is installed at the configured root ("
                       + cosmosRootProvider_() + ") and the path is correct";
    }
    return r;
}

// ── L3: COSMOS logic (API) ────────────────────────────────────────────────────

HealthCheckResult DoctorService::checkApiReachable()
{
    HealthCheckResult r;
    r.checkId  = "api_reachable";
    r.name     = "COSMOS API";
    r.category = "COSMOS";

    if (!api_.isConnected())
        return apiUnavailable("api_reachable", "COSMOS API");

    auto res = api_.get(Core::Api::Routes::kInternalHealth, {});
    if (res) {
        r.status = HealthStatus::Pass;
        r.detail = "API responded at " + api_.config().baseUrl;
        return r;
    }

    if (res.statusCode == 401 || res.statusCode == 403) {
        r.status     = HealthStatus::Fail;
        r.detail     = "API rejected our credentials (HTTP "
                     + std::to_string(res.statusCode) + ")";
        r.suggestion = "The profile's API password must match "
                       "OPENC3_API_PASSWORD in the COSMOS .env file.";
    } else if (res.statusCode == 0) {
        r.status     = HealthStatus::Fail;
        r.detail     = "Could not reach " + api_.config().baseUrl
                     + ": " + res.errorMessage;
        r.suggestion = "Check that traefik is running and the port is correct.";
    } else {
        r.status = HealthStatus::Fail;
        r.detail = "API returned HTTP " + std::to_string(res.statusCode);
    }
    return r;
}

HealthCheckResult DoctorService::checkScope()
{
    HealthCheckResult r;
    r.checkId  = "scope";
    r.name     = "COSMOS scope";
    r.category = "COSMOS";

    if (!api_.isConnected())
        return apiUnavailable("scope", "COSMOS scope");

    auto res = api_.get(Core::Api::Routes::kScopes, {});
    if (!res) {
        r.status = HealthStatus::Fail;
        r.detail = "Could not list scopes";
        const auto message = apiMessage(res.body);
        if (!message.empty()) r.detail += ": " + message;
        return r;
    }

    const std::string& configured = api_.config().scope;
    try {
        const auto parsed = json::parse(res.body);
        if (parsed.is_array()) {
            for (const auto& entry : parsed) {
                if (entry.is_string() && entry.get<std::string>() == configured) {
                    r.status = HealthStatus::Pass;
                    r.detail = "Scope '" + configured + "' exists";
                    return r;
                }
            }
        }
    } catch (const json::exception& e) {
        r.status = HealthStatus::Skipped;
        r.detail = std::string("Could not parse scope list: ") + e.what();
        return r;
    }

    // Every API call carries this scope, so a missing one makes all COSMOS
    // operations fail in ways that look unrelated.
    r.status     = HealthStatus::Fail;
    r.detail     = "Configured scope '" + configured + "' does not exist in COSMOS";
    r.suggestion = "Set the profile's scope to one that exists (usually DEFAULT).";
    return r;
}

HealthCheckResult DoctorService::checkPlugins()
{
    HealthCheckResult r;
    r.checkId  = "plugins";
    r.name     = "Installed plugins";
    r.category = "COSMOS";

    if (!api_.isConnected())
        return apiUnavailable("plugins", "Installed plugins");

    auto res = api_.get(Core::Api::Routes::plugin("all"), {});
    if (!res) {
        r.status = HealthStatus::Fail;
        r.detail = "Could not list plugins";
        const auto message = apiMessage(res.body);
        if (!message.empty()) r.detail += ": " + message;
        return r;
    }

    try {
        const auto parsed = json::parse(res.body);
        const auto count  = parsed.is_object() ? parsed.size() : 0U;
        if (count == 0) {
            // Not a fault - a fresh COSMOS legitimately has none.
            r.status = HealthStatus::Warning;
            r.detail = "No plugins installed";
            r.suggestion = "Install a plugin from the Workspace view.";
        } else {
            r.status = HealthStatus::Pass;
            r.detail = std::to_string(count) + " plugin(s) installed";
        }
    } catch (const json::exception& e) {
        r.status = HealthStatus::Skipped;
        r.detail = std::string("Could not parse plugin list: ") + e.what();
    }
    return r;
}

HealthCheckResult DoctorService::checkMicroservices()
{
    HealthCheckResult r;
    r.checkId  = "microservices";
    r.name     = "Microservices";
    r.category = "COSMOS";

    if (!api_.isConnected())
        return apiUnavailable("microservices", "Microservices");

    auto res = api_.get(Core::Api::Routes::kMicroservices, {});
    if (!res) {
        r.status = HealthStatus::Fail;
        r.detail = "Could not list microservices";
        const auto message = apiMessage(res.body);
        if (!message.empty()) r.detail += ": " + message;
        return r;
    }

    std::vector<std::string> names;
    try {
        const auto parsed = json::parse(res.body);
        // index renders `names(...)`, an array of strings.
        if (parsed.is_array()) {
            for (const auto& entry : parsed)
                if (entry.is_string()) names.push_back(entry.get<std::string>());
        } else if (parsed.is_object()) {
            for (const auto& [key, _] : parsed.items())
                names.push_back(key);
        }
    } catch (const json::exception& e) {
        r.status = HealthStatus::Skipped;
        r.detail = std::string("Could not parse microservice list: ") + e.what();
        return r;
    }

    if (names.empty()) {
        // This is the classic post-install failure: the plugin installed
        // but the operator never started its microservices, so nothing
        // actually runs while the plugin list still looks correct.
        r.status     = HealthStatus::Warning;
        r.detail     = "No microservices are defined";
        r.suggestion = "If you have plugins installed, the operator may have "
                       "failed to start them - check: docker logs <operator>";
        return r;
    }

    // Beyond "are any defined" (the count-only check above): each
    // microservice_status#show reports the runtime state a Ruby thread has
    // reached (MicroserviceStatusModel#state/#error) - a microservice can
    // be fully *defined* and still have crashed after start, which the
    // index route alone cannot reveal.
    std::vector<std::string> errored;
    for (const auto& name : names) {
        auto statusRes = api_.get(Core::Api::Routes::microserviceStatus(name), {});
        if (!statusRes) continue; // Best-effort: one bad lookup shouldn't hide the rest.
        try {
            const auto parsed = json::parse(statusRes.body);
            if (parsed.is_object() && parsed.contains("error")
                && !parsed["error"].is_null())
                errored.push_back(name);
        } catch (const json::exception&) {
            // Not JSON or unexpected shape - skip rather than false-positive.
        }
    }

    if (!errored.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < errored.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += errored[i];
        }
        r.status     = HealthStatus::Fail;
        r.detail     = std::to_string(names.size()) + " microservice(s) defined; "
                       "erroring: " + joined;
        r.suggestion = "Check logs for the listed microservice(s), e.g. "
                       "docker logs <operator> or the microservice's own container.";
        return r;
    }

    r.status = HealthStatus::Pass;
    r.detail = std::to_string(names.size()) + " microservice(s) defined, none reporting an error";
    return r;
}

HealthCheckResult DoctorService::checkInterfaces()
{
    HealthCheckResult r;
    r.checkId  = "interfaces";
    r.name     = "Interfaces";
    r.category = "COSMOS";

    if (!api_.isConnected())
        return apiUnavailable("interfaces", "Interfaces");

    // get_all_interface_info (JSON-DRB) is the only route that carries live
    // CONNECTED/DISCONNECTED state (InterfaceStatusModel) - plain GET
    // /interfaces only renders InterfaceModel, the static configuration, and
    // says nothing about whether the interface's own thread ever connected.
    // Each row is [name, state, clients, txsize, rxsize, txbytes, rxbytes,
    // txcnt, rxcnt, disable_disconnect] per interface_api.rb.
    const auto call = detail::callJsonDrb(
        api_, "get_all_interface_info", json::array(), nextRequestId_.fetch_add(1));

    if (!call.success) {
        // Fall back to the plain REST route so this check still reports
        // *something* useful (defined-but-unknown-state) rather than
        // failing outright if get_all_interface_info is ever unavailable
        // (e.g. an older/newer COSMOS with a different JSON-DRB surface).
        auto res = api_.get(Core::Api::Routes::kInterfaces, {});
        if (!res) {
            r.status = HealthStatus::Fail;
            r.detail = "Could not list interfaces";
            const auto message = apiMessage(res.body);
            if (!message.empty()) r.detail += ": " + message;
            return r;
        }
        try {
            const auto parsed = json::parse(res.body);
            const auto count = parsed.is_array()  ? parsed.size()
                             : parsed.is_object() ? parsed.size()
                                                  : 0U;
            if (count == 0) {
                r.status     = HealthStatus::Warning;
                r.detail     = "No interfaces are defined";
                r.suggestion = "Interfaces come from installed plugins' plugin.txt.";
            } else {
                r.status = HealthStatus::Warning;
                r.detail = std::to_string(count)
                    + " interface(s) defined; live connection state unavailable ("
                    + call.errorMessage + ")";
            }
        } catch (const json::exception& e) {
            r.status = HealthStatus::Skipped;
            r.detail = std::string("Could not parse interface list: ") + e.what();
        }
        return r;
    }

    if (!call.result.is_array() || call.result.empty()) {
        r.status     = HealthStatus::Warning;
        r.detail     = "No interfaces are defined";
        r.suggestion = "Interfaces come from installed plugins' plugin.txt.";
        return r;
    }

    std::vector<std::string> disconnected;
    for (const auto& row : call.result) {
        if (!row.is_array() || row.size() < 2 || !row[0].is_string()) continue;
        const std::string name  = row[0].get<std::string>();
        const std::string state = row[1].is_string() ? row[1].get<std::string>()
                                                       : row[1].dump();
        // ATTEMPTING/CONNECTING are legitimate transient states (interface
        // retrying per its auto_reconnect config), not necessarily a fault -
        // only a settled DISCONNECTED is flagged.
        if (state == "DISCONNECTED")
            disconnected.push_back(name);
    }

    if (!disconnected.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < disconnected.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += disconnected[i];
        }
        r.status     = HealthStatus::Warning;
        r.detail     = std::to_string(call.result.size())
            + " interface(s) defined; disconnected: " + joined;
        r.suggestion = "A disconnected interface receives no telemetry and cannot "
                       "send commands - check its target hardware/network and "
                       "auto_reconnect configuration.";
        return r;
    }

    r.status = HealthStatus::Pass;
    r.detail = std::to_string(call.result.size()) + " interface(s) defined, all connected";
    return r;
}

} // namespace OpenC3::Services
