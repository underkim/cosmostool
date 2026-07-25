#include "SystemService.h"
#include "core/logging/Logger.h"
#include "core/connection/ShellQuote.h"

#include <charconv>
#include <sstream>

namespace OpenC3::Services {

namespace {
// Parses a double, leaving `out` untouched (at its caller-supplied default)
// on any malformed input - top/df output can vary across distros/locales,
// and a bad parse should just skip this one polling tick rather than throw.
void parseDoubleOrLeaveDefault(const std::string& text, double& out)
{
    double parsed{};
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (ec == std::errc{})
        out = parsed;
}
} // namespace

SystemService::SystemService(
    Core::Connection::ICommandExecutor& executor,
    std::function<std::string()>        cosmosRootProvider)
    : executor_(executor)
    , cosmosRootProvider_(std::move(cosmosRootProvider))
{
    if (!cosmosRootProvider_)
        cosmosRootProvider_ = [] { return std::string("/cosmos"); };
}

Models::SystemMetrics SystemService::getMetrics()
{
    Models::SystemMetrics m;

    // ── CPU (1-second snapshot via top) ──────────────────────────────────────
    {
        auto r = executor_.execute(
            "top -bn1 | grep 'Cpu(s)' | awk '{print $2+$4}'");
        if (r && !r.stdOut.empty())
            parseDoubleOrLeaveDefault(r.stdOut, m.cpuPercent);
    }

    // ── Memory (free -m) ─────────────────────────────────────────────────────
    {
        auto r = executor_.execute("free -m | awk '/^Mem:/{print $2,$3,$4}'");
        if (r) {
            std::istringstream ss(r.stdOut);
            double total{}, used{}, free_{};
            ss >> total >> used >> free_;
            m.memTotalMb = total;
            m.memUsedMb  = used;
            m.memPercent = (total > 0) ? (used / total * 100.0) : 0.0;
        }
    }

    // ── Disk (df -h /) ────────────────────────────────────────────────────────
    {
        auto r = executor_.execute("df -BG / | awk 'NR==2{print $2,$3,$4,$5}'");
        if (r) {
            Models::DiskUsage disk;
            disk.mountPoint = "/";
            std::istringstream ss(r.stdOut);
            std::string total, used, avail, pct;
            ss >> total >> used >> avail >> pct;
            auto strip = [](std::string& s){
                s.erase(std::remove(s.begin(), s.end(), 'G'), s.end());
                s.erase(std::remove(s.begin(), s.end(), '%'), s.end());
            };
            strip(total); strip(used); strip(avail); strip(pct);
            parseDoubleOrLeaveDefault(total, disk.totalGb);
            parseDoubleOrLeaveDefault(used,  disk.usedGb);
            parseDoubleOrLeaveDefault(avail, disk.availableGb);
            parseDoubleOrLeaveDefault(pct,   disk.usedPercent);
            m.disks.push_back(disk);
        }
    }

    // ── Hostname ──────────────────────────────────────────────────────────────
    {
        auto r = executor_.execute("hostname");
        if (r) {
            m.hostname = r.stdOut;
            if (!m.hostname.empty() && m.hostname.back() == '\n')
                m.hostname.pop_back();
        }
    }

    return m;
}

std::string SystemService::getOpenC3Version()
{
    // OpenC3 COSMOS stores version in a well-known location relative to the
    // configured COSMOS root (matches DoctorService::checkOpenC3Version()).
    const std::string versionPath =
        cosmosRootProvider_() + "/openc3-cosmos-init/plugins/openc3-tool-base/VERSION";
    auto r = executor_.execute(
        "cat " + Core::Connection::shellQuote(versionPath) + " 2>/dev/null || echo 'unknown'");
    if (!r) return "unknown";
    std::string v = r.stdOut;
    if (!v.empty() && v.back() == '\n') v.pop_back();
    return v;
}

bool SystemService::isOpenC3Running()
{
    // Every COSMOS 6.10.4 Compose service is named openc3-*; Compose may add a
    // configurable project prefix and an instance suffix. Match that stable
    // service-name fragment against unfiltered docker output instead of using
    // `--filter name=cosmos`, which misses minio/redis/traefik/operator and can
    // miss the entire stack when COMPOSE_PROJECT_NAME is not "cosmos".
    auto r = executor_.execute(
        "docker ps --format '{{.Names}}' 2>/dev/null");
    if (!r) return false;

    std::istringstream stream(r.stdOut);
    std::string        name;
    while (std::getline(stream, name)) {
        if (name.find("openc3-") != std::string::npos)
            return true;
    }
    return false;
}

} // namespace OpenC3::Services
