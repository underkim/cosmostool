#include "DockerService.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace OpenC3::Services {

using json = nlohmann::json;
using Core::Connection::shellQuote;

DockerService::DockerService(Core::Connection::ICommandExecutor& executor)
    : executor_(executor)
{}

// ── Container list ────────────────────────────────────────────────────────────

std::vector<Models::DockerContainer>
DockerService::listContainers(bool allContainers)
{
    const std::string flag = allContainers ? "--all " : "";
    // --format with json template gives us machine-readable output
    const std::string cmd =
        "docker ps " + flag +
        "--format '{{json .}}'";

    auto result = executor_.execute(cmd);
    if (!result) {
        Logging::Logger::error("[DockerService] listContainers failed: {}",
                               result.errorMessage);
        return {};
    }

    std::vector<Models::DockerContainer> containers;
    std::istringstream                   stream(result.stdOut);
    std::string                          line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        try {
            containers.push_back(parseContainerLine(line));
        } catch (const std::exception& e) {
            Logging::Logger::warn("[DockerService] Failed to parse line: {}",
                                  e.what());
        }
    }
    return containers;
}

// ── Container actions ─────────────────────────────────────────────────────────

bool DockerService::startContainer(const std::string& nameOrId)
{
    return static_cast<bool>(
        executor_.execute("docker start " + shellQuote(nameOrId)));
}

bool DockerService::stopContainer(const std::string& nameOrId)
{
    return static_cast<bool>(
        executor_.execute("docker stop " + shellQuote(nameOrId)));
}

bool DockerService::restartContainer(const std::string& nameOrId)
{
    return static_cast<bool>(
        executor_.execute("docker restart " + shellQuote(nameOrId)));
}

bool DockerService::removeContainer(const std::string& nameOrId, bool force)
{
    const std::string flag = force ? " -f" : "";
    return static_cast<bool>(
        executor_.execute("docker rm" + flag + " " + shellQuote(nameOrId)));
}

// ── Logs ──────────────────────────────────────────────────────────────────────

std::string DockerService::getLogs(const std::string& nameOrId, int tail)
{
    auto r = executor_.execute(
        "docker logs --tail " + std::to_string(tail) +
        " --timestamps " + shellQuote(nameOrId) + " 2>&1");
    return r ? r.stdOut : r.errorMessage;
}

// ── Stats ─────────────────────────────────────────────────────────────────────

Models::ContainerStats DockerService::getStats(const std::string& nameOrId)
{
    // --no-stream prints one snapshot then exits
    auto r = executor_.execute(
        "docker stats --no-stream --format "
        "'{{json .}}' " + shellQuote(nameOrId));

    Models::ContainerStats stats;
    if (!r) return stats;

    try {
        const auto j = json::parse(r.stdOut);
        // Docker stats output: CPUPerc "0.5%", MemUsage "100MiB / 4GiB"
        std::string cpu = j.value("CPUPerc", "0%");
        // Drop a trailing '%' if present. erase(npos) throws, so guard the find.
        if (const auto pct = cpu.find('%'); pct != std::string::npos)
            cpu.erase(pct);
        stats.cpuPercent = std::stod(cpu);

        std::string mem = j.value("MemUsage", "0MiB / 0GiB");
        // Parse "XXX MiB / YYY GiB" etc. — simplified parsing for skeleton
        std::istringstream ms(mem);
        double used{}; std::string unit{};
        ms >> used >> unit;
        stats.memUsageMb = (unit == "GiB") ? used * 1024.0 : used;

    } catch (const std::exception& e) {
        Logging::Logger::warn("[DockerService] getStats parse error: {}", e.what());
    }
    return stats;
}

// ── Compose ───────────────────────────────────────────────────────────────────

std::vector<Models::ComposeService>
DockerService::listComposeServices(const std::string& composeDir)
{
    auto r = executor_.execute(
        "cd " + shellQuote(composeDir) + " && docker compose ps --format json");
    if (!r) return {};

    std::vector<Models::ComposeService> services;
    std::istringstream                  stream(r.stdOut);
    std::string                         line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        try {
            const auto j = json::parse(line);
            Models::ComposeService svc;
            svc.name       = j.value("Service", "");
            svc.image      = j.value("Image", "");
            svc.statusText = j.value("State", "");
            svc.status     = parseStatus(svc.statusText);
            services.push_back(std::move(svc));
        } catch (const std::exception& e) {
            Logging::Logger::warn("[DockerService] listComposeServices parse error: {}", e.what());
        }
    }
    return services;
}

bool DockerService::composeUp(const std::string& composeDir)
{
    return static_cast<bool>(executor_.execute(
        "cd " + shellQuote(composeDir) + " && docker compose up -d"));
}

bool DockerService::composeDown(const std::string& composeDir)
{
    return static_cast<bool>(executor_.execute(
        "cd " + shellQuote(composeDir) + " && docker compose down"));
}

// ── System info ───────────────────────────────────────────────────────────────

bool DockerService::isDockerRunning()
{
    return static_cast<bool>(executor_.execute("docker info > /dev/null 2>&1"));
}

std::string DockerService::dockerVersion()
{
    auto r = executor_.execute("docker --version");
    return r ? r.stdOut : std::string{};
}

// ── Private ───────────────────────────────────────────────────────────────────

Models::ContainerStatus
DockerService::parseStatus(const std::string& s) const
{
    if (s.find("running") != std::string::npos || s == "running")
        return Models::ContainerStatus::Running;
    if (s.find("exited") != std::string::npos)
        return Models::ContainerStatus::Exited;
    if (s.find("paused") != std::string::npos)
        return Models::ContainerStatus::Paused;
    if (s.find("restarting") != std::string::npos)
        return Models::ContainerStatus::Restarting;
    if (s.find("dead") != std::string::npos)
        return Models::ContainerStatus::Dead;
    return Models::ContainerStatus::Unknown;
}

Models::DockerContainer
DockerService::parseContainerLine(const std::string& jsonLine) const
{
    const auto j = json::parse(jsonLine);
    Models::DockerContainer c;
    c.id          = j.value("ID", "");
    c.shortId     = c.id.substr(0, std::min<std::size_t>(12, c.id.size()));
    // docker ps names include leading slash
    c.name        = j.value("Names", "");
    if (!c.name.empty() && c.name[0] == '/')
        c.name.erase(0, 1);
    c.image       = j.value("Image", "");
    c.command     = j.value("Command", "");
    c.created     = j.value("CreatedAt", "");
    c.statusText  = j.value("Status", "");
    c.status      = parseStatus(j.value("State", ""));
    return c;
}

} // namespace OpenC3::Services
