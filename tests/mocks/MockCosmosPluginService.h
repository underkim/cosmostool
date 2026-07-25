#pragma once

#include "services/plugin/ICosmosPluginService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::ICosmosPluginService (installed plugins in a
/// running COSMOS, API transport).
class MockCosmosPluginService : public Services::ICosmosPluginService {
public:
    MOCK_METHOD(std::vector<Models::Plugin>, listInstalled, (), (override));

    MOCK_METHOD(Services::PluginOperationResult, uploadGem,
                (const std::string&), (override));

    MOCK_METHOD(Services::PluginOperationResult, installUploaded,
                (const std::string&, const std::string&), (override));

    MOCK_METHOD(Services::PluginOperationResult, installGem,
                (const std::string&), (override));

    MOCK_METHOD(Services::PluginOperationResult, uninstall,
                (const std::string&), (override));

    MOCK_METHOD(bool, isAvailable, (), (const, override));
};

} // namespace OpenC3::Tests
