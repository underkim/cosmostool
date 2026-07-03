#pragma once

#include "ISystemService.h"
#include "core/connection/ICommandExecutor.h"

#include <functional>
#include <string>

namespace OpenC3::Services {

class SystemService final : public ISystemService {
public:
    explicit SystemService(
        Core::Connection::ICommandExecutor& executor,
        std::function<std::string()>        cosmosRootProvider = {});

    [[nodiscard]] Models::SystemMetrics getMetrics()         override;
    [[nodiscard]] std::string           getOpenC3Version()   override;
    [[nodiscard]] bool                  isOpenC3Running()    override;

private:
    Core::Connection::ICommandExecutor& executor_;
    std::function<std::string()>        cosmosRootProvider_;
};

} // namespace OpenC3::Services
