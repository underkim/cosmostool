#pragma once

#include "models/DockerContainer.h"

#include <functional>
#include <string>
#include <vector>

namespace OpenC3::Services {

class IDockerService {
public:
    virtual ~IDockerService() = default;

    [[nodiscard]] virtual std::vector<Models::DockerContainer> listContainers(
        bool allContainers = true) = 0;

    [[nodiscard]] virtual bool startContainer  (const std::string& nameOrId) = 0;
    [[nodiscard]] virtual bool stopContainer   (const std::string& nameOrId) = 0;
    [[nodiscard]] virtual bool restartContainer(const std::string& nameOrId) = 0;
    [[nodiscard]] virtual bool removeContainer (const std::string& nameOrId,
                                                bool force = false) = 0;

    [[nodiscard]] virtual std::string getLogs(
        const std::string& nameOrId, int tail = 200) = 0;

    [[nodiscard]] virtual Models::ContainerStats getStats(
        const std::string& nameOrId) = 0;

    [[nodiscard]] virtual std::vector<Models::ComposeService>
        listComposeServices(const std::string& composeDir) = 0;

    [[nodiscard]] virtual bool composeUp  (const std::string& composeDir) = 0;
    [[nodiscard]] virtual bool composeDown(const std::string& composeDir) = 0;

    [[nodiscard]] virtual bool isDockerRunning() = 0;
    [[nodiscard]] virtual std::string dockerVersion() = 0;
};

} // namespace OpenC3::Services
