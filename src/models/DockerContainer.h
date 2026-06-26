#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

enum class ContainerStatus {
    Running,
    Exited,
    Paused,
    Restarting,
    Dead,
    Unknown
};

struct ContainerPort {
    std::string ip;
    int         publicPort{0};
    int         privatePort{0};
    std::string type;   // "tcp" | "udp"
};

struct ContainerStats {
    double cpuPercent{0.0};
    double memUsageMb{0.0};
    double memLimitMb{0.0};
    double memPercent{0.0};
    double netInputMb{0.0};
    double netOutputMb{0.0};
};

struct DockerContainer {
    std::string              id;
    std::string              shortId;       // first 12 chars of id
    std::string              name;
    std::string              image;
    std::string              command;
    std::string              created;
    ContainerStatus          status{ContainerStatus::Unknown};
    std::string              statusText;
    std::vector<ContainerPort> ports;

    [[nodiscard]] bool isRunning() const noexcept {
        return status == ContainerStatus::Running;
    }
};

struct ComposeService {
    std::string     name;
    std::string     image;
    ContainerStatus status{ContainerStatus::Unknown};
    std::string     statusText;
    int             replicaCount{0};
};

} // namespace OpenC3::Models
