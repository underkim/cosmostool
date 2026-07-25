#pragma once

#include "core/api/ICosmosApiClient.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Core::Api::ICosmosApiClient.
///
/// The API counterpart to MockCommandExecutor. Services that talk to COSMOS
/// depend on the interface rather than the Qt implementation, so they can be
/// tested with no network, no Qt event loop, and no running COSMOS instance -
/// which is the whole reason the interface lives in Qt-free core/.
class MockCosmosApiClient : public Core::Api::ICosmosApiClient {
public:
    MOCK_METHOD(void, configure, (const Core::Api::ApiConfig&), (override));
    MOCK_METHOD(const Core::Api::ApiConfig&, config, (), (const, noexcept, override));

    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, noexcept, override));
    MOCK_METHOD(std::string, lastError, (), (const, override));

    MOCK_METHOD(Core::Api::ApiResponse, get,
                (const std::string&, const Core::Api::QueryParams&), (override));

    MOCK_METHOD(Core::Api::ApiResponse, post,
                (const std::string&, const std::string&, const Core::Api::QueryParams&),
                (override));

    MOCK_METHOD(Core::Api::ApiResponse, postForm,
                (const std::string&, const Core::Api::QueryParams&,
                 const Core::Api::QueryParams&),
                (override));

    MOCK_METHOD(Core::Api::ApiResponse, postFile,
                (const std::string&, const std::string&, const std::string&,
                 const Core::Api::QueryParams&, const Core::Api::QueryParams&),
                (override));

    MOCK_METHOD(Core::Api::ApiResponse, del,
                (const std::string&, const Core::Api::QueryParams&), (override));
};

} // namespace OpenC3::Tests
