#include "DoctorService.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"

namespace OpenC3::Services {

using Models::HealthCheckResult;
using Models::HealthStatus;
using Models::DoctorReport;
using Core::Connection::shellQuote;

DoctorService::DoctorService(
    Core::Connection::ICommandExecutor& executor,
    std::function<std::string()>        cosmosRootProvider)
    : executor_(executor)
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

    add("docker_installed",   [this]{ return checkDockerInstalled();   });
    add("docker_running",     [this]{ return checkDockerRunning();     });
    add("docker_compose",     [this]{ return checkDockerCompose();     });
    add("cpu_usage",          [this]{ return checkCpuUsage();          });
    add("memory_usage",       [this]{ return checkMemoryUsage();       });
    add("disk_space",         [this]{ return checkDiskSpace();         });
    add("cosmos_containers",  [this]{ return checkOpenC3Containers();  });
    add("network",            [this]{ return checkNetworkConnectivity();});
    add("time_sync",          [this]{ return checkTimeSynchronization();});
    add("plugin_folder",      [this]{ return checkOpenC3PluginFolder();});
    add("openc3_version",     [this]{ return checkOpenC3Version();     });
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

// ── Individual checks ─────────────────────────────────────────────────────────

HealthCheckResult DoctorService::checkDockerInstalled()
{
    HealthCheckResult r;
    r.checkId  = "docker_installed";
    r.name     = "Docker installed";
    r.category = "Docker";

    auto res = executor_.execute("which docker 2>/dev/null || command -v docker");
    if (res && !res.stdOut.empty()) {
        r.status = HealthStatus::Pass;
        r.detail = "docker found at " + res.stdOut;
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
        r.detail = res.stdOut;
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
    try { cpu = std::stod(res.stdOut); } catch (...) {}
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
    try { pct = std::stod(res.stdOut); } catch (...) {}
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
    try { pct = std::stoi(res.stdOut); } catch (...) {}
    r.detail = std::to_string(pct) + "% used";

    if (pct < 80)      r.status = HealthStatus::Pass;
    else if (pct < 90) r.status = HealthStatus::Warning;
    else {
        r.status     = HealthStatus::Fail;
        r.suggestion = "Free disk space. OpenC3 logs and images can fill quickly.";
    }
    return r;
}

HealthCheckResult DoctorService::checkOpenC3Containers()
{
    HealthCheckResult r;
    r.checkId  = "cosmos_containers";
    r.name     = "OpenC3 containers";
    r.category = "OpenC3";

    auto res = executor_.execute(
        "docker ps --filter 'name=cosmos' --format '{{.Names}}' | wc -l");
    if (!res) {
        r.status = HealthStatus::Skipped;
        r.detail = "Docker not available";
        return r;
    }
    int count = 0;
    try { count = std::stoi(res.stdOut); } catch (...) {}

    if (count >= 5) {
        r.status = HealthStatus::Pass;
        r.detail = std::to_string(count) + " OpenC3 containers running";
    } else if (count > 0) {
        r.status     = HealthStatus::Warning;
        r.detail     = "Only " + std::to_string(count) + " OpenC3 containers running";
        r.suggestion = "Run: docker compose up -d in your OpenC3 directory";
    } else {
        r.status     = HealthStatus::Fail;
        r.detail     = "No OpenC3 containers running";
        r.suggestion = "Start OpenC3 with: docker compose up -d";
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

HealthCheckResult DoctorService::checkOpenC3PluginFolder()
{
    HealthCheckResult r;
    r.checkId  = "plugin_folder";
    r.name     = "OpenC3 plugin folder";
    r.category = "OpenC3";

    const std::string pluginPath = cosmosRootProvider_() + "/plugins";
    auto res = executor_.execute(
        "test -d " + shellQuote(pluginPath) + " && echo EXISTS || echo MISSING");

    if (res && res.stdOut.find("EXISTS") != std::string::npos) {
        r.status = HealthStatus::Pass;
        r.detail = "Plugin folder found at " + pluginPath;
    } else {
        r.status     = HealthStatus::Warning;
        r.detail     = "Plugin folder not found at " + pluginPath;
        r.suggestion = "Verify your OpenC3 installation path "
                       "(configured root: " + cosmosRootProvider_() + ")";
    }
    return r;
}

HealthCheckResult DoctorService::checkOpenC3Version()
{
    HealthCheckResult r;
    r.checkId  = "openc3_version";
    r.name     = "OpenC3 version";
    r.category = "OpenC3";

    const std::string versionPath =
        cosmosRootProvider_() +
        "/openc3-cosmos-init/plugins/openc3-tool-base/VERSION";
    auto res = executor_.execute(
        "cat " + shellQuote(versionPath) + " 2>/dev/null || echo unknown");

    if (res && res.stdOut != "unknown\n" && !res.stdOut.empty()) {
        r.status = HealthStatus::Pass;
        r.detail = "OpenC3 version: " + res.stdOut;
    } else {
        r.status     = HealthStatus::Warning;
        r.detail     = "Could not determine OpenC3 version";
        r.suggestion = "Ensure OpenC3 is installed at the configured root ("
                       + cosmosRootProvider_() + ") and the path is correct";
    }
    return r;
}

} // namespace OpenC3::Services
