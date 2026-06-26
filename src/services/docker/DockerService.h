#pragma once

#include "IDockerService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

class DockerService final : public IDockerService {
public:
    explicit DockerService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] std::vector<Models::DockerContainer> listContainers(
        bool allContainers) override;

    [[nodiscard]] bool startContainer  (const std::string& nameOrId) override;
    [[nodiscard]] bool stopContainer   (const std::string& nameOrId) override;
    [[nodiscard]] bool restartContainer(const std::string& nameOrId) override;
    [[nodiscard]] bool removeContainer (const std::string& nameOrId,
                                        bool force) override;

    [[nodiscard]] std::string getLogs(
        const std::string& nameOrId, int tail) override;

    [[nodiscard]] Models::ContainerStats getStats(
        const std::string& nameOrId) override;

    [[nodiscard]] std::vector<Models::ComposeService>
        listComposeServices(const std::string& composeDir) override;

    [[nodiscard]] bool composeUp  (const std::string& composeDir) override;
    [[nodiscard]] bool composeDown(const std::string& composeDir) override;

    [[nodiscard]] bool        isDockerRunning() override;
    [[nodiscard]] std::string dockerVersion()   override;

private:
    [[nodiscard]] Models::ContainerStatus parseStatus(const std::string& s) const;
    [[nodiscard]] Models::DockerContainer parseContainerLine(
        const std::string& jsonLine) const;

    Core::Connection::ICommandExecutor& executor_;
};

} // namespace OpenC3::Services
