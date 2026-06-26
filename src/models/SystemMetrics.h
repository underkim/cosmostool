#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

struct DiskUsage {
    std::string filesystem;
    std::string mountPoint;
    double      totalGb{0.0};
    double      usedGb{0.0};
    double      availableGb{0.0};
    double      usedPercent{0.0};
};

struct SystemMetrics {
    double cpuPercent{0.0};
    double memUsedMb{0.0};
    double memTotalMb{0.0};
    double memPercent{0.0};
    double swapUsedMb{0.0};
    double swapTotalMb{0.0};

    std::vector<DiskUsage> disks;

    std::string   hostname;
    std::string   osRelease;
    std::string   kernelVersion;
    int           uptimeSeconds{0};

    [[nodiscard]] double memFreePercent() const noexcept {
        return 100.0 - memPercent;
    }
};

} // namespace OpenC3::Models
