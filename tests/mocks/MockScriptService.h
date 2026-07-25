#pragma once

#include "services/script/IScriptService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::IScriptService (COSMOS Script Runner, API
/// transport).
class MockScriptService : public Services::IScriptService {
public:
    MOCK_METHOD(std::vector<Models::CosmosScript>, listScripts,
                (const std::string&), (override));

    MOCK_METHOD(Models::ScriptRunResult, run,
                (const std::string&), (override));

    MOCK_METHOD(Models::ScriptExecutionStatus, status,
                (const std::string&), (override));

    MOCK_METHOD(bool, stop, (const std::string&), (override));
    MOCK_METHOD(bool, resume, (const std::string&), (override));
    MOCK_METHOD(bool, pause, (const std::string&), (override));

    MOCK_METHOD(bool, isAvailable, (), (const, override));
};

} // namespace OpenC3::Tests
