#pragma once

#include "ISystemService.h"
#include "core/connection/ICommandExecutor.h"

namespace OpenC3::Services {

class SystemService final : public ISystemService {
public:
    explicit SystemService(Core::Connection::ICommandExecutor& executor);

    [[nodiscard]] Models::SystemMetrics getMetrics()         override;
    [[nodiscard]] std::string           getOpenC3Version()   override;
    [[nodiscard]] bool                  isOpenC3Running()    override;

private:
    Core::Connection::ICommandExecutor& executor_;
};

} // namespace OpenC3::Services
