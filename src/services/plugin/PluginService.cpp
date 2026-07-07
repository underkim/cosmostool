#include "PluginService.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <string_view>

namespace OpenC3::Services {

using Core::Connection::shellQuote;

namespace {

std::string normalizeCosmosRoot(std::string path)
{
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();

    constexpr std::string_view scriptName = "/openc3.sh";
    if (path == "openc3.sh")
        return ".";

    if (path.size() >= scriptName.size()
        && path.compare(path.size() - scriptName.size(), scriptName.size(), scriptName) == 0)
    {
        const auto slash = path.rfind('/');
        return slash == std::string::npos || slash == 0 ? "/" : path.substr(0, slash);
    }

    return path.empty() ? "/cosmos" : path;
}

} // namespace

PluginService::PluginService(Core::Connection::ICommandExecutor& executor)
    : executor_(executor)
{}

std::vector<Models::Plugin> PluginService::listInstalled(const std::string& cosmosRoot)
{
    const std::string pluginsDir = normalizeCosmosRoot(cosmosRoot) + "/plugins";
    auto r = executor_.execute(
        "plugins_dir=" + shellQuote(pluginsDir) + "; "
        "if [ ! -d \"$plugins_dir\" ]; then exit 0; fi; "
        "for d in \"$plugins_dir\"/*; do "
        "  [ -d \"$d\" ] || continue; "
        "  name=$(basename \"$d\"); "
        "  plugin_txt=\"\"; [ -f \"$d/plugin.txt\" ] && plugin_txt=\"$d/plugin.txt\"; "
        "  gemspec=$(find \"$d\" -maxdepth 1 -type f -name '*.gemspec' 2>/dev/null | head -n 1); "
        "  gem=$(find \"$d\" -maxdepth 1 -type f -name '*.gem' 2>/dev/null | head -n 1); "
        "  targets=$(find \"$d/targets\" -mindepth 1 -maxdepth 1 -type d -printf '%f,' 2>/dev/null); "
        "  printf '%s\\t%s\\t%s\\t%s\\t%s\\n' \"$name\" \"$d\" \"$plugin_txt\" \"$gemspec\" \"$gem|$targets\"; "
        "done");

    std::vector<Models::Plugin> plugins;
    if (!r) return plugins;

    std::istringstream stream(r.stdOut);
    std::string        line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::stringstream lineStream(line);
        std::string field;
        while (std::getline(lineStream, field, '\t'))
            fields.push_back(field);
        if (fields.size() < 5) continue;

        Models::Plugin p;
        p.name          = fields[0];
        p.rootPath      = fields[1];
        p.pluginTxtPath = fields[2];
        p.gemspecPath   = fields[3];

        const std::size_t sep = fields[4].find('|');
        p.gemFilePath = sep == std::string::npos ? fields[4] : fields[4].substr(0, sep);
        const std::string targets = sep == std::string::npos ? std::string{} : fields[4].substr(sep + 1);
        std::stringstream targetStream(targets);
        std::string target;
        while (std::getline(targetStream, target, ',')) {
            if (target.empty()) continue;
            Models::PluginTarget t;
            t.name = target;
            p.targets.push_back(std::move(t));
        }

        p.status = (!p.pluginTxtPath.empty() && !p.gemspecPath.empty())
            ? Models::PluginStatus::Installed
            : Models::PluginStatus::Error;
        plugins.push_back(std::move(p));
    }
    return plugins;
}

Models::PluginValidationResult
PluginService::validate(const std::string& localPluginPath)
{
    Models::PluginValidationResult result;
    result.valid = true;

    auto r = executor_.execute(
        "p=" + shellQuote(localPluginPath) + "; "
        "if [ -d \"$p\" ]; then "
        "  [ -f \"$p/plugin.txt\" ] || echo MISSING_PLUGIN_TXT; "
        "  find \"$p\" -maxdepth 1 -type f -name '*.gemspec' | grep -q . || echo MISSING_GEMSPEC; "
        "  [ -d \"$p/targets\" ] || echo MISSING_TARGETS; "
        "elif [ -f \"$p\" ]; then "
        "  echo OK; "
        "else "
        "  echo MISSING_PATH; "
        "fi");

    if (!r || r.stdOut.find("MISSING_PATH") != std::string::npos) {
        Models::PluginValidationIssue issue;
        issue.severity = Models::PluginValidationIssue::Severity::Error;
        issue.code     = "FILE_NOT_FOUND";
        issue.message  = "Plugin file not found: " + localPluginPath;
        result.issues.push_back(issue);
        result.valid = false;
    }
    if (r) {
        auto addIssue = [&](const std::string& code, const std::string& message) {
            if (r.stdOut.find(code) == std::string::npos) return;
            Models::PluginValidationIssue issue;
            issue.severity = Models::PluginValidationIssue::Severity::Error;
            issue.code     = code;
            issue.message  = message;
            issue.filePath = localPluginPath;
            result.issues.push_back(issue);
            result.valid = false;
        };
        addIssue("MISSING_PLUGIN_TXT", "plugin.txt is missing.");
        addIssue("MISSING_GEMSPEC", "A .gemspec file is missing.");
        addIssue("MISSING_TARGETS", "targets directory is missing.");
    }
    return result;
}

bool PluginService::install(
    const std::string& gemFilePath, const std::string& cosmosRoot)
{
    const std::string root = normalizeCosmosRoot(cosmosRoot);
    Logging::Logger::info("[PluginService] Installing plugin: {}", gemFilePath);
    auto r = executor_.execute(
        "cp " + shellQuote(gemFilePath) + " " +
        shellQuote(root + "/plugins/") + " 2>&1");
    if (!r)
        Logging::Logger::warn("[PluginService] Install failed for {}: {}",
                               gemFilePath, r.stdOut.empty() ? r.errorMessage : r.stdOut);
    return static_cast<bool>(r);
}

bool PluginService::remove(
    const std::string& pluginName, const std::string& cosmosRoot)
{
    const std::string root = normalizeCosmosRoot(cosmosRoot);
    Logging::Logger::info("[PluginService] Removing plugin: {}", pluginName);
    auto r = executor_.execute(
        "rm -f " + shellQuote(root + "/plugins/" + pluginName + ".gem") +
        " 2>&1");
    if (!r)
        Logging::Logger::warn("[PluginService] Remove failed for {}: {}",
                               pluginName, r.stdOut.empty() ? r.errorMessage : r.stdOut);
    return static_cast<bool>(r);
}

std::string PluginService::build(const std::string& pluginRootPath)
{
    // PluginViewModel::build() decides success/failure by sniffing this
    // string for "ERROR"/"failed" (there's no separate success flag on this
    // interface) - so every failure path here must both (a) actually reach
    // the caller (previously a missing gemspec or a missing `gem` binary
    // produced no output at all, since `&&` short-circuited the whole
    // pipeline silently) and (b) say "ERROR" so it isn't mistaken for a
    // real gem path and reported as a successful build.
    auto r = executor_.execute(
        "cd " + shellQuote(pluginRootPath) +
        " && if ! command -v gem >/dev/null 2>&1; then"
        "      echo 'ERROR: gem (RubyGems) is not installed on this host - Ruby is required to build a plugin.';"
        "      exit 1;"
        "    fi"
        " && gemspec=$(find . -maxdepth 1 -type f -name '*.gemspec' | head -n 1)"
        " && if [ -z \"$gemspec\" ]; then"
        "      echo 'ERROR: No .gemspec file found in the plugin folder.';"
        "      exit 1;"
        "    fi"
        " && gem build \"$gemspec\" 2>&1"
        " && find . -maxdepth 1 -type f -name '*.gem' -printf '%p\\n' 2>/dev/null");

    // The command above always echoes an "ERROR: ..." line (or the real
    // gem-build output) to stdout before failing, via the explicit checks
    // and gem build's own 2>&1 redirect - so stdOut, not the generic
    // errorMessage fallback, is what actually carries the useful diagnostic.
    if (!r.stdOut.empty()) return r.stdOut;
    return r ? std::string{} : r.errorMessage;
}

bool PluginService::backup(
    const std::string& pluginName,
    const std::string& localBackupPath,
    const std::string& cosmosRoot)
{
    const std::string root = normalizeCosmosRoot(cosmosRoot);
    return executor_.downloadFile(
        root + "/plugins/" + pluginName + ".gem", localBackupPath);
}

} // namespace OpenC3::Services
