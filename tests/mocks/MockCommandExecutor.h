#pragma once

#include "core/connection/ICommandExecutor.h"
#include <gmock/gmock.h>

namespace OpenC3::Tests {

class MockCommandExecutor : public Core::Connection::ICommandExecutor {
public:
    MOCK_METHOD(bool, connect,    (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, noexcept, override));
    MOCK_METHOD(std::string, lastError, (), (const, override));

    MOCK_METHOD(Core::Connection::ExecutorResult,
                execute, (const std::string&), (override));

    MOCK_METHOD(Core::Connection::ExecutorResult,
                executeStreaming,
                (const std::string&,
                 std::function<void(const std::string&)>),
                (override));

    MOCK_METHOD(bool, uploadFile,
                (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, downloadFile,
                (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, fileExists,
                (const std::string&), (override));
    MOCK_METHOD(std::string, readFile,
                (const std::string&), (override));
    MOCK_METHOD(bool, writeFile,
                (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::vector<std::string>, listDirectory,
                (const std::string&), (override));
};

} // namespace OpenC3::Tests
