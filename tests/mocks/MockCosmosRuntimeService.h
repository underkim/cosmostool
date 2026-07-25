#pragma once

#include "services/runtime/ICosmosRuntimeService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::ICosmosRuntimeService (read-only COSMOS
/// target/command/telemetry state, JSON-DRB API transport).
class MockCosmosRuntimeService : public Services::ICosmosRuntimeService {
public:
    MOCK_METHOD(std::vector<Models::RuntimeTarget>, listTargets, (), (override));

    MOCK_METHOD(std::vector<Models::RuntimePacket>, listCommands,
                (const std::string&), (override));

    MOCK_METHOD(std::vector<Models::RuntimePacket>, listTelemetry,
                (const std::string&), (override));

    MOCK_METHOD(std::vector<Models::TelemetryItemValue>, telemetryValues,
                (const std::string&, const std::string&), (override));

    MOCK_METHOD(bool, isAvailable, (), (const, override));
};

} // namespace OpenC3::Tests
