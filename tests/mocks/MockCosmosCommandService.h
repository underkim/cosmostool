#pragma once

#include "services/runtime/ICosmosCommandService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::ICosmosCommandService (COSMOS command
/// sending, JSON-DRB API transport).
class MockCosmosCommandService : public Services::ICosmosCommandService {
public:
    MOCK_METHOD(Models::CommandSendResult, send,
                (const std::string&, const std::string&,
                 const std::vector<Services::CommandParameter>&), (override));

    MOCK_METHOD(Models::CommandSendResult, sendConfirmed,
                (const std::string&, const std::string&,
                 const std::vector<Services::CommandParameter>&), (override));

    MOCK_METHOD(bool, isAvailable, (), (const, override));
};

} // namespace OpenC3::Tests
