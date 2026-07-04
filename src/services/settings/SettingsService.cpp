#include "SettingsService.h"
#include "core/logging/Logger.h"

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#  include <dpapi.h>
#  include <wincrypt.h>
#endif

namespace OpenC3::Services {

using json = nlohmann::json;

namespace {

constexpr std::string_view kDpapiPrefix = "enc:v1:dpapi:";
constexpr std::string_view kObfPrefix = "enc:v1:obf:";
constexpr std::string_view kObfKey = "OpenC3DevToolkitSettingsSecret";
constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<unsigned char>& bytes)
{
    std::string out;
    out.reserve(((bytes.size() + 2U) / 3U) * 4U);

    for (std::size_t i = 0; i < bytes.size(); i += 3U) {
        const unsigned int b0 = bytes[i];
        const unsigned int b1 = (i + 1U < bytes.size()) ? bytes[i + 1U] : 0U;
        const unsigned int b2 = (i + 2U < bytes.size()) ? bytes[i + 2U] : 0U;
        const unsigned int triple = (b0 << 16U) | (b1 << 8U) | b2;

        out.push_back(kBase64Alphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(kBase64Alphabet[(triple >> 12U) & 0x3FU]);
        out.push_back((i + 1U < bytes.size())
            ? kBase64Alphabet[(triple >> 6U) & 0x3FU]
            : '=');
        out.push_back((i + 2U < bytes.size())
            ? kBase64Alphabet[triple & 0x3FU]
            : '=');
    }

    return out;
}

int base64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<unsigned char> base64Decode(std::string_view text)
{
    std::vector<unsigned char> out;
    int val = 0;
    int valb = -8;
    for (const char c : text) {
        if (c == '=') break;
        const int decoded = base64Value(c);
        if (decoded < 0) return {};
        val = (val << 6) + decoded;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::vector<unsigned char> xorWithFallbackKey(std::string_view secret)
{
    std::vector<unsigned char> bytes;
    bytes.reserve(secret.size());
    for (std::size_t i = 0; i < secret.size(); ++i) {
        bytes.push_back(static_cast<unsigned char>(
            static_cast<unsigned char>(secret[i]) ^
            static_cast<unsigned char>(kObfKey[i % kObfKey.size()])));
    }
    return bytes;
}

std::string fallbackProtect(std::string_view secret)
{
    if (secret.empty()) return {};
    Logging::Logger::warn(
        "[SettingsService] DPAPI unavailable; storing SSH secret with reversible obfuscation only.");
    return std::string(kObfPrefix) + base64Encode(xorWithFallbackKey(secret));
}

std::string fallbackUnprotect(std::string_view encoded)
{
    const auto bytes = base64Decode(encoded);
    if (bytes.empty() && !encoded.empty()) return {};

    std::string out;
    out.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out.push_back(static_cast<char>(
            bytes[i] ^ static_cast<unsigned char>(kObfKey[i % kObfKey.size()])));
    }
    return out;
}

#ifdef _WIN32
std::string dpapiProtect(std::string_view secret)
{
    if (secret.empty()) return {};

    DATA_BLOB plain{};
    plain.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(secret.data()));
    plain.cbData = static_cast<DWORD>(secret.size());

    DATA_BLOB encrypted{};
    if (!CryptProtectData(&plain, L"OpenC3DevToolkit", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &encrypted)) {
        Logging::Logger::error(
            "[SettingsService] CryptProtectData failed; falling back to obfuscation.");
        return fallbackProtect(secret);
    }

    std::vector<unsigned char> bytes(encrypted.pbData,
                                     encrypted.pbData + encrypted.cbData);
    LocalFree(encrypted.pbData);
    return std::string(kDpapiPrefix) + base64Encode(bytes);
}

std::string dpapiUnprotect(std::string_view encoded)
{
    const auto bytes = base64Decode(encoded);
    if (bytes.empty() && !encoded.empty()) return {};

    DATA_BLOB encrypted{};
    encrypted.pbData = const_cast<BYTE*>(bytes.data());
    encrypted.cbData = static_cast<DWORD>(bytes.size());

    DATA_BLOB plain{};
    if (!CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &plain)) {
        Logging::Logger::error("[SettingsService] CryptUnprotectData failed.");
        return {};
    }

    std::string out(reinterpret_cast<char*>(plain.pbData), plain.cbData);
    LocalFree(plain.pbData);
    return out;
}
#endif

std::string protectSecret(const std::string& secret)
{
#ifdef _WIN32
    return dpapiProtect(secret);
#else
    return fallbackProtect(secret);
#endif
}

std::string unprotectSecret(const std::string& stored)
{
    if (stored.empty()) return {};

    if (stored.rfind(kObfPrefix, 0) == 0) {
        return fallbackUnprotect(std::string_view(stored).substr(kObfPrefix.size()));
    }

    if (stored.rfind(kDpapiPrefix, 0) == 0) {
#ifdef _WIN32
        return dpapiUnprotect(std::string_view(stored).substr(kDpapiPrefix.size()));
#else
        Logging::Logger::error(
            "[SettingsService] Cannot decrypt Windows DPAPI SSH secret on this platform.");
        return {};
#endif
    }

    // Backward compatibility: pre-encryption settings.json files stored secrets
    // directly. Return the value so existing profiles continue to work; the next
    // saveProfile() call will rewrite it with a protected prefix.
    Logging::Logger::warn(
        "[SettingsService] Loaded legacy plaintext SSH secret; it will be protected on next save.");
    return stored;
}

} // namespace


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
        {"password",        protectSecret(p.password)},
        {"privateKeyPath",  p.privateKeyPath},
        {"publicKeyPath",   p.publicKeyPath},
        {"passphrase",      protectSecret(p.passphrase)},
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
    p.password        = unprotectSecret(j.value("password", ""));
    p.privateKeyPath  = j.value("privateKeyPath", "");
    p.publicKeyPath   = j.value("publicKeyPath", "");
    p.passphrase      = unprotectSecret(j.value("passphrase", ""));
    p.connectTimeoutMs= j.value("connectTimeoutMs", 10000);
    p.commandTimeoutMs= j.value("commandTimeoutMs", 30000);
    return p;
}

} // namespace OpenC3::Services
