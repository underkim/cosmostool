#pragma once

#include "ISettingsService.h"
#include <nlohmann/json.hpp>
#include <string>

namespace OpenC3::Services {

/// Persists settings to a JSON file in the user's AppData / home directory.
class SettingsService final : public ISettingsService {
public:
    explicit SettingsService(std::string settingsFilePath);

    // ── ISettingsService ──────────────────────────────────────────────────────
    [[nodiscard]] std::vector<Models::ConnectionProfile> profiles() const override;
    [[nodiscard]] std::optional<Models::ConnectionProfile>
        profileById(const std::string& id) const override;
    [[nodiscard]] std::optional<Models::ConnectionProfile>
        defaultProfile() const override;

    void saveProfile(const Models::ConnectionProfile& profile) override;
    void deleteProfile(const std::string& id) override;
    void setDefaultProfile(const std::string& id) override;

    [[nodiscard]] std::string getString(
        const std::string& key, const std::string& fallback) const override;
    [[nodiscard]] int    getInt(const std::string& key, int fallback)   const override;
    [[nodiscard]] bool   getBool(const std::string& key, bool fallback) const override;

    void setString(const std::string& key, const std::string& value) override;
    void setInt(const std::string& key, int value) override;
    void setBool(const std::string& key, bool value) override;

    void load() override;
    void save() override;

private:
    [[nodiscard]] nlohmann::json profileToJson(
        const Models::ConnectionProfile& p) const;
    [[nodiscard]] Models::ConnectionProfile profileFromJson(
        const nlohmann::json& j) const;

    std::string      filePath_;
    nlohmann::json   data_;   // { "profiles": [...], "settings": {...} }
};

} // namespace OpenC3::Services
