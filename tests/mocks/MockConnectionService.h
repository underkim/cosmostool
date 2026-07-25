#pragma once

#include "services/connection/IConnectionService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::IConnectionService.
class MockConnectionService : public Services::IConnectionService {
public:
    MOCK_METHOD(bool, connect, (const std::string&), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(Services::ConnectionState, state, (), (const, noexcept, override));

    MOCK_METHOD(const Models::ConnectionProfile*, activeProfile, (),
                (const, noexcept, override));

    MOCK_METHOD(std::string, cosmosRootPath, (), (const, noexcept, override));

    // SubscriptionId is a member typedef of IConnectionService, not a
    // namespace-level type - qualifying it as Services::SubscriptionId does
    // not compile.
    MOCK_METHOD(Services::IConnectionService::SubscriptionId, onStateChanged,
                (std::function<void(const Services::ConnectionEvent&)>), (override));

    MOCK_METHOD(void, removeStateChanged,
                (Services::IConnectionService::SubscriptionId), (override));
};

} // namespace OpenC3::Tests
