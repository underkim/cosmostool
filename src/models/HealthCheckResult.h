#pragma once

#include <string>
#include <vector>

namespace OpenC3::Models {

enum class HealthStatus {
    Pass,
    Warning,
    Fail,
    Skipped
};

struct HealthCheckResult {
    std::string  checkId;      // stable identifier used for test lookup
    std::string  name;         // Human-readable name
    std::string  category;     // "Docker", "System", "Network", "OpenC3"
    HealthStatus status{HealthStatus::Fail};
    std::string  detail;       // What was checked / what was found
    std::string  suggestion;   // Actionable remediation hint

    [[nodiscard]] bool passed()  const noexcept { return status == HealthStatus::Pass;    }
    [[nodiscard]] bool warned()  const noexcept { return status == HealthStatus::Warning; }
    [[nodiscard]] bool failed()  const noexcept { return status == HealthStatus::Fail;    }
    [[nodiscard]] bool skipped() const noexcept { return status == HealthStatus::Skipped; }
};

struct DoctorReport {
    std::vector<HealthCheckResult> results;

    [[nodiscard]] int countByStatus(HealthStatus s) const noexcept {
        int n = 0;
        for (const auto& r : results)
            if (r.status == s) ++n;
        return n;
    }

    [[nodiscard]] bool allPassed() const noexcept {
        return countByStatus(HealthStatus::Fail) == 0
            && countByStatus(HealthStatus::Warning) == 0;
    }
};

} // namespace OpenC3::Models
