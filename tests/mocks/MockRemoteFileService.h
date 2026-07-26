#pragma once

#include "services/filesystem/IRemoteFileService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::IRemoteFileService.
class MockRemoteFileService : public Services::IRemoteFileService {
public:
    MOCK_METHOD(std::vector<std::string>, listDirectory,
                (const std::string&), (override));

    MOCK_METHOD(std::string, readFile, (const std::string&), (override));

    MOCK_METHOD(bool, writeFile,
                (const std::string&, const std::string&), (override));

    MOCK_METHOD(bool, fileExists, (const std::string&), (override));

    MOCK_METHOD(std::string, executeCommand, (const std::string&), (override));

    MOCK_METHOD(void, streamCommand,
                (const std::string&, std::function<void(const std::string&)>),
                (override));

    MOCK_METHOD(void, cancelStreamingCommand, (), (override));
};

} // namespace OpenC3::Tests
