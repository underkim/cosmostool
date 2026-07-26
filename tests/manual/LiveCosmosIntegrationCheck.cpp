// Live-COSMOS integration probe.
//
// Not part of the unit suite (mock-based) and not the localhost-HTTP-server
// transport probe (tests/manual/HttpCosmosApiClientLiveCheck.cpp). This one
// requires a real COSMOS 6.10.4 stack reachable at the URL given on argv[1]
// (default http://127.0.0.1:2900) and drives the actual production service
// classes (DoctorService, CosmosRuntimeService, CosmosPluginService,
// CosmosCommandService, CosmosScriptService) through HttpCosmosApiClient
// against it - the closest thing to an end-to-end smoke test this client
// has without a GUI.
//
// Authentication is intentionally accepted only via OPENC3_LIVE_PASSWORD so
// the credential is not embedded in this source or exposed in a process-list
// command line.
// Usage: OPENC3_LIVE_PASSWORD=... opencosmos_live_cosmos_check [baseUrl] [scope]
#include "viewmodels/api/HttpCosmosApiClient.h"
#include "core/api/CosmosRoutes.h"
#include "services/docker/DockerService.h"
#include "services/doctor/DoctorService.h"
#include "services/runtime/CosmosRuntimeService.h"
#include "services/runtime/CosmosCommandService.h"
#include "services/runtime/detail/JsonDrbCall.h"
#include "services/plugin/CosmosPluginService.h"
#include "services/script/CosmosScriptService.h"
#include "services/workspace/WorkspaceService.h"
#include "core/connection/NullCommandExecutor.h"
#include "core/connection/ShellQuote.h"
#include "core/connection/wsl/WslExecutor.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using OpenC3::Core::Api::ApiConfig;
using OpenC3::ViewModels::Api::HttpCosmosApiClient;
using namespace OpenC3::Services;
using namespace OpenC3::Models;
namespace Core = OpenC3::Core;
using json = nlohmann::json;

namespace {

int failures = 0;

void check(bool condition, const std::string& label)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << label << "\n";
    if (!condition) ++failures;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const char* passwordEnv = std::getenv("OPENC3_LIVE_PASSWORD");
    if (passwordEnv == nullptr || *passwordEnv == '\0') {
        std::cerr << "OPENC3_LIVE_PASSWORD is required\n";
        return 2;
    }

    const std::string baseUrl = argc > 1 ? argv[1] : "http://127.0.0.1:2900";
    const std::string scope   = argc > 2 ? argv[2] : "DEFAULT";
    const char* distroEnv = std::getenv("OPENC3_LIVE_WSL_DISTRO");
    const char* rootEnv   = std::getenv("OPENC3_LIVE_COSMOS_ROOT");
    const char* mutatingEnv = std::getenv("OPENC3_LIVE_MUTATING");
    const std::string distro = distroEnv != nullptr && *distroEnv != '\0'
        ? distroEnv : "Ubuntu-24.04";
    const std::string cosmosRoot = rootEnv != nullptr && *rootEnv != '\0'
        ? rootEnv : "/mnt/c/cosmos/cosmos";
    const bool allowMutating = mutatingEnv != nullptr
        && std::string(mutatingEnv) == "1";

    std::cout << "Target: " << baseUrl << " (scope=" << scope << ")\n\n";

    ApiConfig config;
    config.baseUrl = baseUrl;
    config.password = passwordEnv;
    config.scope = scope;

    HttpCosmosApiClient api(config);
    Core::Connection::NullCommandExecutor shell;

    // ── ICosmosApiClient basics ────────────────────────────────────────────
    std::cout << "-- connectivity --\n";
    check(api.connect(), "connect() authenticates against the real scopes route");
    check(api.isConnected(), "isConnected() true after a successful probe");
    {
        auto res = api.get(Core::Api::Routes::kInternalHealth, {});
        check(static_cast<bool>(res), "GET /internal/health succeeds");
    }
    {
        auto res = api.get(Core::Api::Routes::kScopes, {});
        check(static_cast<bool>(res), "GET /scopes succeeds");
        check(res.body.find(scope) != std::string::npos,
              "configured scope appears in the scopes list");
    }

#ifdef _WIN32
    // ── WslExecutor + Doctor L1/L2 against the real WSL deployment ─────────
    std::cout << "\n-- WslExecutor + DoctorService L1/L2 --\n";
    Core::Connection::ConnectionConfig shellConfig;
    shellConfig.mode = Core::Connection::ConnectionMode::WSL;
    shellConfig.wslDistribution = distro;
    shellConfig.cosmosRootPath = cosmosRoot;
    Core::Connection::WslExecutor liveShell(shellConfig);
    check(liveShell.connect(), "WslExecutor connects to " + distro);
    if (liveShell.isConnected()) {
        const auto dockerPs = liveShell.execute(
            "docker ps --format '{{.Names}}'");
        check(static_cast<bool>(dockerPs), "docker ps executes through WslExecutor");
        check(dockerPs.stdOut.find("openc3-cosmos-cmd-tlm-api") != std::string::npos,
              "real COSMOS cmd-tlm-api container is visible through WSL");

        DockerService docker(liveShell);
        const auto containers = docker.listContainers(true);
        bool cosmosContainerFound = false;
        for (const auto& container : containers)
            if (container.name.find("openc3-cosmos-cmd-tlm-api")
                != std::string::npos)
                cosmosContainerFound = true;
        check(!containers.empty(),
              "DockerService parses docker ps JSON through WslExecutor");
        check(cosmosContainerFound,
              "DockerService returns the real COSMOS cmd-tlm-api container");

        check(liveShell.fileExists(cosmosRoot + "/compose.yaml"),
              "configured COSMOS root is readable through WSL file transport");

        DoctorService fullDoctor(
            liveShell, api, [cosmosRoot] { return cosmosRoot; });
        for (const char* checkId : {
                 "docker_installed", "docker_running", "docker_compose",
                 "cosmos_containers", "redis", "redis_ephemeral", "minio"}) {
            const auto result = fullDoctor.runCheck(checkId);
            std::cout << "         " << checkId << ": " << result.detail << "\n";
            check(result.status == HealthStatus::Pass,
                  std::string("Doctor ") + checkId + ": Pass");
        }
    }
#endif

    // ── DoctorService: real L1(shell via Null executor, expected all-fail)+L3 ──
    std::cout << "\n-- DoctorService (L3 checks against the real API) --\n";
    {
        DoctorService doctor(shell, api);
        auto apiReachable = doctor.runCheck("api_reachable");
        check(apiReachable.status == HealthStatus::Pass,
              "api_reachable: Pass (real traefik+cmd-tlm-api are up)");

        auto scopeCheck = doctor.runCheck("scope");
        check(scopeCheck.status == HealthStatus::Pass,
              "scope: Pass (DEFAULT exists on a real 6.10.4 stack)");

        auto plugins = doctor.runCheck("plugins");
        check(plugins.status == HealthStatus::Pass,
              "plugins: Pass (this stack has plugins installed)");
        std::cout << "         detail: " << plugins.detail << "\n";

        auto microservices = doctor.runCheck("microservices");
        std::cout << "         microservices detail: " << microservices.detail << "\n";
        check(microservices.status == HealthStatus::Pass
                  || microservices.status == HealthStatus::Fail,
              "microservices: reaches a real per-service status verdict (not Skipped)");

        auto interfaces = doctor.runCheck("interfaces");
        std::cout << "         interfaces detail: " << interfaces.detail << "\n";
        check(interfaces.status == HealthStatus::Pass
                  || interfaces.status == HealthStatus::Warning,
              "interfaces: JSON-DRB get_all_interface_info round-trips against a real stack");
    }

    // ── CosmosRuntimeService: real targets/commands/telemetry ─────────────────
    std::cout << "\n-- CosmosRuntimeService (JSON-DRB against the real API) --\n";
    {
        CosmosRuntimeService runtime(api);
        check(runtime.isAvailable(), "isAvailable() true");

        auto targets = runtime.listTargets();
        check(!targets.empty(), "listTargets() returns at least one real target");
        std::cout << "         targets:";
        for (const auto& t : targets) std::cout << " " << t.name;
        std::cout << "\n";

        if (!targets.empty()) {
            const auto& firstTarget = targets.front().name;
            auto cmds = runtime.listCommands(firstTarget);
            auto tlm  = runtime.listTelemetry(firstTarget);
            check(!cmds.empty() || !tlm.empty(),
                  "listCommands/listTelemetry return real packets for " + firstTarget);
            std::cout << "         " << firstTarget << ": " << cmds.size()
                      << " command(s), " << tlm.size() << " telemetry packet(s)\n";

            if (!tlm.empty()) {
                auto values = runtime.telemetryValues(
                    firstTarget, tlm.front().packetName);
                check(!values.empty(),
                      "telemetryValues() returns real item values for "
                          + firstTarget + " " + tlm.front().packetName);
            }
        }
    }

    // ── CosmosPluginService: real installed-plugin list ────────────────────
    std::cout << "\n-- CosmosPluginService --\n";
    {
        CosmosPluginService plugins(api);
        auto installed = plugins.listInstalled();
        check(!installed.empty(), "listInstalled() returns real installed plugins");
        std::cout << "         " << installed.size() << " plugin(s) installed\n";
    }

#ifdef _WIN32
    // ── Optional mutating lifecycle: build -> install -> poll -> uninstall ─
    if (allowMutating && liveShell.isConnected()) {
        std::cout << "\n-- Mutating plugin lifecycle (dedicated temporary plugin) --\n";
        constexpr const char* kGemBase = "openc3-cosmos-hermes-livecheck-0.0.1.gem";
        // Hidden so WorkspaceService::listSources()'s normal `plugins/*`
        // scan does not present this transient probe as a developer project.
        // The same folder is mounted at /plugins inside cmd-tlm-api, which
        // supplies a RubyGems fallback when the WSL host itself lacks Ruby.
        const std::string remoteRoot =
            cosmosRoot + "/plugins/.opencosmos-hermes-livecheck";

        WorkspaceService workspace(liveShell);
        CosmosPluginService plugins(api);

        auto waitForJob = [&plugins](const std::string& processName) {
            PluginJobStatus last;
            for (int attempt = 0; attempt < 120; ++attempt) {
                last = plugins.jobStatus(processName);
                if (last.found && last.isTerminal()) return last;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            return last;
        };

        // Clean up only an artifact bearing this probe's reserved name from a
        // previous interrupted run; never touch user plugins.
        for (const auto& installed : plugins.listInstalled()) {
            if (!installed.name.starts_with(kGemBase)) continue;
            const auto remove = plugins.uninstall(installed.name);
            if (remove.success && !remove.processName.empty())
                (void)waitForJob(remove.processName);
        }

        const std::string setupCommand =
            "rm -rf " + Core::Connection::shellQuote(remoteRoot)
            + " && mkdir -p " + Core::Connection::shellQuote(
                remoteRoot + "/targets/HERMES_LIVECHECK/procedures")
            + " " + Core::Connection::shellQuote(
                remoteRoot + "/targets/HERMES_LIVECHECK/cmd_tlm")
            + " " + Core::Connection::shellQuote(remoteRoot + "/lib");
        check(static_cast<bool>(liveShell.execute(setupCommand)),
              "temporary WSL plugin directory created");

        const std::string gemspec =
            "Gem::Specification.new do |s|\n"
            "  s.name = 'openc3-cosmos-hermes-livecheck'\n"
            "  s.version = '0.0.1'\n"
            "  s.summary = 'OpenC3 Developer Toolkit live integration probe'\n"
            "  s.description = s.summary\n"
            "  s.authors = ['OpenC3 Developer Toolkit']\n"
            "  s.email = ['noreply@example.invalid']\n"
            "  s.files = Dir['plugin.txt', 'targets/**/*', 'lib/**/*']\n"
            "  s.licenses = ['MIT']\n"
            "  s.required_ruby_version = '>= 3.1'\n"
            "end\n";
        check(liveShell.writeFile(
                  remoteRoot + "/openc3-cosmos-hermes-livecheck.gemspec", gemspec),
              "temporary gemspec written through WslExecutor");
        check(liveShell.writeFile(
                  remoteRoot + "/plugin.txt",
                  "VARIABLE hermes_livecheck_marker verified\n"
                  "TARGET HERMES_LIVECHECK HERMES_LIVECHECK\n"
                  "INTERFACE HERMES_LIVECHECK_INT "
                  "hermes_livecheck_interface.rb\n"
                  "  MAP_TARGET HERMES_LIVECHECK\n"),
              "temporary plugin.txt written through WslExecutor");
        check(liveShell.writeFile(
                  remoteRoot + "/lib/hermes_livecheck_interface.rb",
                  "require 'openc3/interfaces/interface'\n\n"
                  "module OpenC3\n"
                  "  class HermesLivecheckInterface < Interface\n"
                  "    def initialize\n"
                  "      super()\n"
                  "      @connected = false\n"
                  "      @read_allowed = false\n"
                  "    end\n\n"
                  "    def connect\n"
                  "      super()\n"
                  "      @connected = true\n"
                  "    end\n\n"
                  "    def connected?\n"
                  "      @connected\n"
                  "    end\n\n"
                  "    def disconnect\n"
                  "      super()\n"
                  "      @connected = false\n"
                  "    end\n\n"
                  "    def write_interface(data, _extra = nil)\n"
                  "      data\n"
                  "    end\n"
                  "  end\n"
                  "end\n"),
              "temporary in-memory command interface written");
        check(liveShell.writeFile(
                  remoteRoot
                      + "/targets/HERMES_LIVECHECK/procedures/hermes_livecheck.rb",
                  "puts 'OpenC3 Developer Toolkit live Script Runner check'\n"),
              "harmless temporary Script Runner procedure written");
        check(liveShell.writeFile(
                  remoteRoot
                      + "/targets/HERMES_LIVECHECK/procedures/hermes_control.rb",
                  "40.times do |index|\n"
                  "  puts \"OpenC3 live control check #{index}\"\n"
                  "  sleep 0.5\n"
                  "end\n"),
              "temporary long-running control procedure written");
        check(liveShell.writeFile(
                  remoteRoot
                      + "/targets/HERMES_LIVECHECK/cmd_tlm/hermes_cmds.txt",
                  "COMMAND HERMES_LIVECHECK NOOP BIG_ENDIAN "
                  "\"Toolkit in-memory interface live-check command\"\n"
                  "  APPEND_PARAMETER VALUE 8 UINT 0 255 7 "
                  "\"Harmless marker value\"\n\n"
                  "COMMAND HERMES_LIVECHECK HAZARDOUS_NOOP BIG_ENDIAN "
                  "\"Toolkit hazardous-confirmation live check\"\n"
                  "  HAZARDOUS \"Dedicated no-interface integration test\"\n"
                  "  APPEND_PARAMETER VALUE 8 UINT 0 255 9 "
                  "\"Harmless marker value\"\n"),
              "temporary in-memory command definitions written");

        const auto built = workspace.buildArtifact(remoteRoot);
        std::string remoteGemPath;
        if (built.success) {
            remoteGemPath = built.remoteGemPath;
            check(true, "WorkspaceService::buildArtifact succeeds in WSL");
        } else {
            std::cout << "         host build detail: " << built.output << "\n";
            check(built.output.find("Ruby is required") != std::string::npos,
                  "WorkspaceService reports the missing host Ruby prerequisite");

            // Keep exercising the real upload/install pipeline using the
            // RubyGems already present in the COSMOS API container. This is
            // test scaffolding only, not a production Workspace fallback.
            const auto containerBuild = liveShell.execute(
                "api_container=$(docker ps --format '{{.Names}}' "
                "| grep 'openc3-cosmos-cmd-tlm-api' | head -n 1)"
                " && test -n \"$api_container\""
                " && docker exec -w /plugins/.opencosmos-hermes-livecheck "
                "\"$api_container\" gem build "
                "openc3-cosmos-hermes-livecheck.gemspec 2>&1");
            remoteGemPath = remoteRoot + "/" + kGemBase;
            check(containerBuild && liveShell.fileExists(remoteGemPath),
                  "COSMOS container RubyGems builds the temporary artifact");
        }

        QTemporaryDir localDir;
        check(localDir.isValid(), "local temporary artifact directory created");
        const std::string localGem =
            (localDir.path() + "/" + kGemBase).toStdString();
        if (!remoteGemPath.empty() && localDir.isValid()) {
            check(workspace.downloadArtifact(remoteGemPath, localGem),
                  "WorkspaceService::downloadArtifact copies gem to Windows");

            const auto uploaded = plugins.uploadGem(localGem);
            check(uploaded.success, "phase 1 gem upload succeeds");

            std::string pluginId;
            if (uploaded.success) {
                try {
                    pluginId = json::parse(uploaded.message).value("name", "");
                } catch (const json::exception&) {
                    // The assertion below reports the malformed response.
                }
            }
            check(!pluginId.empty(), "phase 1 returns the COSMOS instance id");

            if (!pluginId.empty()) {
                const auto install = plugins.installUploaded(pluginId, uploaded.message);
                check(install.success && !install.processName.empty(),
                      "phase 2 install job starts and returns a process handle");
                if (install.success && !install.processName.empty()) {
                    const auto installStatus = waitForJob(install.processName);
                    std::cout << "         install state: " << installStatus.state
                              << " " << installStatus.detail << "\n";
                    check(installStatus.found && installStatus.completedSuccessfully(),
                          "install ProcessStatus reaches Complete/Warning");
                }

                // Phase 1/2 use the bare gem filename. The async install job
                // assigns the __N instance suffix; discover that exact id from
                // listInstalled() before calling uninstall.
                std::string instanceId;
                for (const auto& item : plugins.listInstalled()) {
                    if (item.name.starts_with(kGemBase)) {
                        instanceId = item.name;
                        break;
                    }
                }
                check(!instanceId.empty(),
                      "installed plugin appears with an assigned instance suffix");

                if (!instanceId.empty()) {
                    CosmosCommandService commands(api);
                    bool commandInterfaceReady = false;
                    for (int attempt = 0; attempt < 120; ++attempt) {
                        const auto interfaces = OpenC3::Services::detail::callJsonDrb(
                            api, "get_all_interface_info", nlohmann::json::array(),
                            8000 + attempt);
                        if (interfaces.success && interfaces.result.is_array()) {
                            for (const auto& row : interfaces.result) {
                                if (row.is_array() && row.size() >= 2
                                    && row[0] == "HERMES_LIVECHECK_INT"
                                    && row[1] == "CONNECTED") {
                                    commandInterfaceReady = true;
                                    break;
                                }
                            }
                        }
                        if (commandInterfaceReady) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    }
                    check(commandInterfaceReady,
                          "temporary in-memory command interface reaches CONNECTED");

                    if (commandInterfaceReady) {
                        const std::vector<CommandParameter> noopParameters{
                            {"VALUE", "42"}};
                        const auto noop = commands.send(
                            "HERMES_LIVECHECK", "NOOP", noopParameters);
                        check(noop.success && !noop.cmdString.empty(),
                              "valid NOOP command sends through the in-memory interface");

                        const auto hazardous = commands.send(
                            "HERMES_LIVECHECK", "HAZARDOUS_NOOP", noopParameters);
                        check(!hazardous.success && hazardous.hazardous
                                  && !hazardous.hazardousDescription.empty(),
                              "hazardous NOOP is refused pending explicit confirmation");
                        if (hazardous.hazardous) {
                            const auto confirmed = commands.sendConfirmed(
                                "HERMES_LIVECHECK", "HAZARDOUS_NOOP",
                                noopParameters);
                            check(confirmed.success && !confirmed.cmdString.empty(),
                                  "confirmed hazardous NOOP sends only after refusal");
                        }
                    }

                    CosmosScriptService scripts(api);
                    constexpr const char* kScriptPath =
                        "HERMES_LIVECHECK/procedures/hermes_livecheck.rb";
                    bool scriptVisible = false;
                    for (const auto& script : scripts.listScripts("HERMES_LIVECHECK"))
                        if (script.path == kScriptPath) scriptVisible = true;
                    check(scriptVisible,
                          "temporary procedure appears through script-api");

                    if (scriptVisible) {
                        const auto run = scripts.run(kScriptPath);
                        check(run.success && !run.runningScriptId.empty(),
                              "harmless Script Runner job starts");
                        if (run.success && !run.runningScriptId.empty()) {
                            ScriptExecutionStatus scriptStatus;
                            for (int attempt = 0; attempt < 120; ++attempt) {
                                scriptStatus = scripts.status(run.runningScriptId);
                                if (scriptStatus.found && scriptStatus.isTerminal()) break;
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(250));
                            }
                            std::cout << "         script state: "
                                      << scriptStatus.state << "\n";
                            check(scriptStatus.found
                                      && scriptStatus.completedSuccessfully(),
                                  "Script Runner status reaches completed");
                        }
                    }

                    constexpr const char* kControlScriptPath =
                        "HERMES_LIVECHECK/procedures/hermes_control.rb";
                    const auto controlRun = scripts.run(kControlScriptPath);
                    check(controlRun.success && !controlRun.runningScriptId.empty(),
                          "long-running Script Runner job starts");
                    if (controlRun.success && !controlRun.runningScriptId.empty()) {
                        ScriptExecutionStatus controlStatus;
                        for (int attempt = 0; attempt < 40; ++attempt) {
                            controlStatus = scripts.status(controlRun.runningScriptId);
                            if (controlStatus.found
                                && controlStatus.state == "running") break;
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }
                        check(controlStatus.found
                                  && controlStatus.state == "running",
                              "control script reaches running state");

                        check(scripts.pause(controlRun.runningScriptId),
                              "pause endpoint accepts the running script");
                        for (int attempt = 0; attempt < 80; ++attempt) {
                            controlStatus = scripts.status(controlRun.runningScriptId);
                            if (controlStatus.found
                                && controlStatus.state == "paused") break;
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }
                        check(controlStatus.found
                                  && controlStatus.state == "paused",
                              "Script Runner status reaches paused");

                        check(scripts.resume(controlRun.runningScriptId),
                              "resume endpoint accepts the paused script");
                        for (int attempt = 0; attempt < 40; ++attempt) {
                            controlStatus = scripts.status(controlRun.runningScriptId);
                            if (controlStatus.found
                                && controlStatus.state == "running") break;
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }
                        check(controlStatus.found
                                  && controlStatus.state == "running",
                              "Script Runner returns to running after resume");

                        check(scripts.stop(controlRun.runningScriptId),
                              "stop endpoint accepts the running script");
                        for (int attempt = 0; attempt < 80; ++attempt) {
                            controlStatus = scripts.status(controlRun.runningScriptId);
                            if (controlStatus.found && controlStatus.isTerminal()) break;
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }
                        std::cout << "         stopped script state: "
                                  << controlStatus.state << "\n";
                        check(controlStatus.found && controlStatus.isTerminal(),
                              "stopped Script Runner job reaches a terminal state");
                    }

                    const auto remove = plugins.uninstall(instanceId);
                    check(remove.success && !remove.processName.empty(),
                          "uninstall exact instance starts and returns a process handle");
                    if (remove.success && !remove.processName.empty()) {
                        const auto removeStatus = waitForJob(remove.processName);
                        std::cout << "         uninstall state: " << removeStatus.state
                                  << " " << removeStatus.detail << "\n";
                        check(removeStatus.found && removeStatus.completedSuccessfully(),
                              "uninstall ProcessStatus reaches Complete/Warning");
                    }
                }

                bool remains = false;
                for (const auto& item : plugins.listInstalled())
                    if (item.name.starts_with(kGemBase)) remains = true;
                check(!remains, "temporary plugin is absent after uninstall");

                bool targetRemains = true;
                bool interfaceRemains = true;
                bool scriptRemains = true;
                bool targetObserved = false;
                bool interfaceObserved = false;
                bool scriptObserved = false;
                for (int attempt = 0; attempt < 120; ++attempt) {
                    targetObserved = false;
                    const auto targets = OpenC3::Services::detail::callJsonDrb(
                        api, "get_target_names", nlohmann::json::array(),
                        8500 + attempt);
                    if (targets.success && targets.result.is_array()) {
                        targetObserved = true;
                        targetRemains = false;
                        for (const auto& target : targets.result)
                            if (target.is_string()
                                && target == "HERMES_LIVECHECK")
                                targetRemains = true;
                    }

                    interfaceObserved = false;
                    const auto interfaces = OpenC3::Services::detail::callJsonDrb(
                        api, "get_all_interface_info", nlohmann::json::array(),
                        9000 + attempt);
                    if (interfaces.success && interfaces.result.is_array()) {
                        interfaceObserved = true;
                        interfaceRemains = false;
                        for (const auto& row : interfaces.result)
                            if (row.is_array() && !row.empty()
                                && row[0] == "HERMES_LIVECHECK_INT")
                                interfaceRemains = true;
                    }

                    scriptObserved = false;
                    const auto scriptResponse = api.get(
                        Core::Api::Routes::kScripts,
                        {{"target", "HERMES_LIVECHECK"}});
                    if (scriptResponse) {
                        try {
                            const auto parsed = nlohmann::json::parse(
                                scriptResponse.body);
                            if (parsed.is_array()) {
                                scriptObserved = true;
                                scriptRemains = false;
                                for (const auto& script : parsed)
                                    if (script.is_string()
                                        && script.get<std::string>().find(
                                               "HERMES_LIVECHECK/")
                                            != std::string::npos)
                                        scriptRemains = true;
                            }
                        } catch (const nlohmann::json::exception&) {
                            // Unknown, not absent: retry until a parse succeeds.
                        }
                    }

                    if (targetObserved && interfaceObserved && scriptObserved
                        && !targetRemains && !interfaceRemains && !scriptRemains)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
                check(targetObserved && !targetRemains,
                      "temporary target is observed absent after uninstall");
                check(interfaceObserved && !interfaceRemains,
                      "temporary command interface is absent after uninstall");
                check(scriptObserved && !scriptRemains,
                      "temporary scripts are observed absent after uninstall");
            }
        }

        // Best-effort cleanup if an assertion above failed after COSMOS had
        // already created the instance. Restricted to the probe's reserved
        // gem prefix, never user plugins.
        for (const auto& item : plugins.listInstalled()) {
            if (!item.name.starts_with(kGemBase)) continue;
            const auto remove = plugins.uninstall(item.name);
            if (remove.success && !remove.processName.empty())
                (void)waitForJob(remove.processName);
        }

        (void)liveShell.execute(
            "rm -rf " + Core::Connection::shellQuote(remoteRoot));
    } else {
        std::cout << "\n-- Mutating plugin lifecycle skipped "
                     "(set OPENC3_LIVE_MUTATING=1 to enable) --\n";
    }
#endif

    // ── CosmosCommandService: production error path, deliberately no send ──
    std::cout << "\n-- CosmosCommandService (nonexistent command only) --\n";
    {
        CosmosCommandService commands(api);
        check(commands.isAvailable(), "isAvailable() true");
        // A deliberately nonexistent target/command exercises the actual
        // cmd JSON-DRB request and error decoding but cannot enqueue a command
        // to any configured interface or external device.
        const auto result = commands.send(
            "OPENCOSMOS_LIVE_CHECK_NO_SUCH_TARGET",
            "NO_SUCH_COMMAND", {});
        check(!result.success,
              "nonexistent command is rejected cleanly without a live send");
        check(!result.errorMessage.empty(),
              "COSMOS command rejection preserves an actionable message");
    }

    // ── CosmosScriptService: script-api reachability (list only, no run) ──
    std::cout << "\n-- CosmosScriptService --\n";
    {
        CosmosScriptService scripts(api);
        auto list = scripts.listScripts();
        std::cout << "         " << list.size() << " script(s) visible in bucket storage\n";
        // Not asserting non-empty: a fresh scope legitimately has none.
        check(true, "listScripts() completes without throwing");
    }

    std::cout << "\n" << (failures == 0 ? "ALL LIVE COSMOS CHECKS PASSED"
                                         : "LIVE COSMOS CHECK FAILURES")
               << "\n";
    return failures == 0 ? 0 : 1;
}
