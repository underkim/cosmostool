#include "SettingsService.h"
#include "core/logging/Logger.h"

#include <fstream>
#include <stdexcept>

namespace OpenC3::Services {

using json = nlohmann::json;

SettingsService::SettingsService(std::string settingsFilePath)
    : filePath_(std::move(settingsFilePath))
{
    data_["profiles"] = json::array();
    data_["settings"] = json::object();
}

// ── Profiles ──────────────────────────────────────────────────────────────────

std::vector<Models::ConnectionProfile> SettingsService::profiles() const
{
    std::vector<Models::ConnectionProfile> result;
    for (const auto& j : data_["profiles"])
        result.push_back(profileFromJson(j));
    return result;
}

std::optional<Models::ConnectionProfile>
SettingsService::profileById(const std::string& id) const
{
    for (const auto& j : data_["profiles"])
        if (j.value("id", "") == id)
            return profileFromJson(j);
    return std::nullopt;
}

std::optional<Models::ConnectionProfile> SettingsService::defaultProfile() const
{
    for (const auto& j : data_["profiles"])
        if (j.value("isDefault", false))
            return profileFromJson(j);
    return std::nullopt;
}

void SettingsService::saveProfile(const Models::ConnectionProfile& profile)
{
    auto& arr = data_["profiles"];
    for (auto& j : arr) {
        if (j.value("id", "") == profile.id) {
            j = profileToJson(profile);
            save();
            return;
        }
    }
    arr.push_back(profileToJson(profile));
    save();
}

void SettingsService::deleteProfile(const std::string& id)
{
    auto& arr = data_["profiles"];
    arr.erase(
        std::remove_if(arr.begin(), arr.end(),
            [&id](const json& j) { return j.value("id", "") == id; }),
        arr.end());
    save();
}

void SettingsService::setDefaultProfile(const std::string& id)
{
    for (auto& j : data_["profiles"])
        j["isDefault"] = (j.value("id", "") == id);
    save();
}

// ── Key-value settings ────────────────────────────────────────────────────────

std::string SettingsService::getString(
    const std::string& key, const std::string& fallback) const
{
    return data_["settings"].value(key, fallback);
}

int SettingsService::getInt(const std::string& key, int fallback) const
{
    return data_["settings"].value(key, fallback);
}

bool SettingsService::getBool(const std::string& key, bool fallback) const
{
    return data_["settings"].value(key, fallback);
}

void SettingsService::setString(const std::string& key, const std::string& value)
{
    data_["settings"][key] = value;
    save();
}

void SettingsService::setInt(const std::string& key, int value)
{
    data_["settings"][key] = value;
    save();
}

void SettingsService::setBool(const std::string& key, bool value)
{
    data_["settings"][key] = value;
    save();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SettingsService::load()
{
    std::ifstream f(filePath_);
    if (!f.is_open()) {
        Logging::Logger::info("[SettingsService] No settings file found at '{}'"
                              "; using defaults.", filePath_);
        return;
    }
    try {
        data_ = json::parse(f);
        if (!data_.contains("profiles")) data_["profiles"] = json::array();
        if (!data_.contains("settings")) data_["settings"] = json::object();
        Logging::Logger::info("[SettingsService] Loaded settings from '{}'",
                              filePath_);
    } catch (const json::exception& e) {
        Logging::Logger::error("[SettingsService] JSON parse error: {}",
                               e.what());
    }
}

void SettingsService::save()
{
    std::ofstream f(filePath_);
    if (!f.is_open()) {
        Logging::Logger::error("[SettingsService] Cannot write to '{}'",
                               filePath_);
        return;
    }
    f << data_.dump(4);
}

// ── JSON serialisation ────────────────────────────────────────────────────────

json SettingsService::profileToJson(const Models::ConnectionProfile& p) const
{
    return {
        {"id",              p.id},
        {"name",            p.name},
        {"mode",            static_cast<int>(p.mode)},
        {"isDefault",       p.isDefault},
        {"cosmosRootPath",  p.cosmosRootPath},
        {"wslDistribution", p.wslDistribution},
        {"host",            p.host},
        {"port",            p.port},
        {"username",        p.username},
        {"authMethod",      static_cast<int>(p.authMethod)},
        {"password",        p.password},
        {"privateKeyPath",  p.privateKeyPath},
        {"connectTimeoutMs",p.connectTimeoutMs},
        {"commandTimeoutMs",p.commandTimeoutMs}
    };
}

Models::ConnectionProfile
SettingsService::profileFromJson(const json& j) const
{
    Models::ConnectionProfile p;
    p.id              = j.value("id", "");
    p.name            = j.value("name", "");
    p.mode            = static_cast<Models::ConnectionMode>(j.value("mode", 0));
    p.isDefault       = j.value("isDefault", false);
    p.cosmosRootPath  = j.value("cosmosRootPath", "/cosmos");
    p.wslDistribution = j.value("wslDistribution", "Ubuntu");
    p.host            = j.value("host", "");
    p.port            = j.value("port", 22);
    p.username        = j.value("username", "");
    p.authMethod      = static_cast<Models::AuthMethod>(j.value("authMethod", 0));
    p.password        = j.value("password", "");
    p.privateKeyPath  = j.value("privateKeyPath", "");
    p.connectTimeoutMs= j.value("connectTimeoutMs", 10000);
    p.commandTimeoutMs= j.value("commandTimeoutMs", 30000);
    return p;
}

} // namespace OpenC3::Services
