#pragma once

#include "models/HealthCheckResult.h"
#include <functional>

namespace OpenC3::Services {

class IDoctorService {
public:
    virtual ~IDoctorService() = default;

    /// Run all health checks and return the full report.
    /// onProgress called with each result as checks complete (for live UI updates).
    [[nodiscard]] virtual Models::DoctorReport runAll(
        std::function<void(const Models::HealthCheckResult&)> onProgress = {}) = 0;

    /// Re-run a single check by its checkId.
    [[nodiscard]] virtual Models::HealthCheckResult
        runCheck(const std::string& checkId) = 0;
};

} // namespace OpenC3::Services
