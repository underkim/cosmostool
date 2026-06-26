#pragma once

#include <functional>
#include <string>
#include <vector>

namespace OpenC3::Services {

/// File-system operations on the remote target.
/// Thin façade over ICommandExecutor's file API.
class IRemoteFileService {
public:
    virtual ~IRemoteFileService() = default;

    [[nodiscard]] virtual std::vector<std::string>
        listDirectory(const std::string& remotePath) = 0;

    [[nodiscard]] virtual std::string readFile(const std::string& remotePath) = 0;

    [[nodiscard]] virtual bool writeFile(const std::string& remotePath,
                                         const std::string& content) = 0;

    [[nodiscard]] virtual bool fileExists(const std::string& remotePath) = 0;

    /// Stream a command's stdout line-by-line (e.g. "tail -f logfile").
    /// Returns when the command finishes or is interrupted.
    virtual void streamCommand(
        const std::string&                      command,
        std::function<void(const std::string&)> onLine) = 0;
};

} // namespace OpenC3::Services
