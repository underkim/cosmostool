#pragma once

#include "models/SystemMetrics.h"
#include <string>

namespace OpenC3::Services {

class ISystemService {
public:
    virtual ~ISystemService() = default;

    [[nodiscard]] virtual Models::SystemMetrics getMetrics() = 0;
    [[nodiscard]] virtual std::string           getOpenC3Version() = 0;
    [[nodiscard]] virtual bool                  isOpenC3Running() = 0;
};

} // namespace OpenC3::Services
