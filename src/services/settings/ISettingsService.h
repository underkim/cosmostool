#pragma once

#include "models/ConnectionProfile.h"

#include <optional>
#include <string>
#include <vector>

namespace OpenC3::Services {

class ISettingsService {
public:
    virtual ~ISettingsService() = default;

    // ── Connection Profiles ───────────────────────────────────────────────────
    [[nodiscard]] virtual std::vector<Models::ConnectionProfile> profiles() const = 0;
    [[nodiscard]] virtual std::optional<Models::ConnectionProfile>
        profileById(const std::string& id) const = 0;
    [[nodiscard]] virtual std::optional<Models::ConnectionProfile>
        defaultProfile() const = 0;

    virtual void saveProfile(const Models::ConnectionProfile& profile) = 0;
    virtual void deleteProfile(const std::string& id) = 0;
    virtual void setDefaultProfile(const std::string& id) = 0;

    // ── Application Settings ──────────────────────────────────────────────────
    [[nodiscard]] virtual std::string getString(
        const std::string& key, const std::string& fallback = {}) const = 0;
    [[nodiscard]] virtual int    getInt(const std::string& key, int fallback = 0)       const = 0;
    [[nodiscard]] virtual bool   getBool(const std::string& key, bool fallback = false) const = 0;

    virtual void setString(const std::string& key, const std::string& value) = 0;
    virtual void setInt(const std::string& key, int value) = 0;
    virtual void setBool(const std::string& key, bool value) = 0;

    virtual void load() = 0;
    virtual void save() = 0;
};

} // namespace OpenC3::Services
