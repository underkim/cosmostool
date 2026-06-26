#include "SystemService.h"
#include "core/logging/Logger.h"

#include <sstream>
#include <regex>

namespace OpenC3::Services {

SystemService::SystemService(Core::Connection::ICommandExecutor& executor)
    : executor_(executor)
{}

Models::SystemMetrics SystemService::getMetrics()
{
    Models::SystemMetrics m;

    // ── CPU (1-second snapshot via top) ──────────────────────────────────────
    {
        auto r = executor_.execute(
            "top -bn1 | grep 'Cpu(s)' | awk '{print $2+$4}'");
        if (r && !r.stdOut.empty()) {
            try { m.cpuPercent = std::stod(r.stdOut); }
            catch (...) {}
        }
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
            try {
                disk.totalGb     = std::stod(total);
                disk.usedGb      = std::stod(used);
                disk.availableGb = std::stod(avail);
                disk.usedPercent = std::stod(pct);
            } catch (...) {}
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
    // OpenC3 COSMOS stores version in a well-known location
    auto r = executor_.execute(
        "cat /cosmos/openc3-cosmos-init/plugins/openc3-tool-base/VERSION"
        " 2>/dev/null || echo 'unknown'");
    if (!r) return "unknown";
    std::string v = r.stdOut;
    if (!v.empty() && v.back() == '\n') v.pop_back();
    return v;
}

bool SystemService::isOpenC3Running()
{
    auto r = executor_.execute(
        "docker ps --filter name=cosmos --format '{{.Names}}' 2>/dev/null"
        " | grep -c cosmos || echo 0");
    return r && r.stdOut != "0\n" && !r.stdOut.empty();
}

} // namespace OpenC3::Services
