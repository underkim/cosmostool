#pragma once

#include "services/workspace/IWorkspaceService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::IWorkspaceService (plugin sources on the host,
/// shell transport).
class MockWorkspaceService : public Services::IWorkspaceService {
public:
    MOCK_METHOD(std::vector<Models::Plugin>, listSources,
                (const std::string&), (override));

    MOCK_METHOD(Models::PluginValidationResult, validate,
                (const std::string&), (override));

    MOCK_METHOD(std::string, build, (const std::string&), (override));

    MOCK_METHOD(Services::PluginBuildResult, buildArtifact,
                (const std::string&), (override));

    MOCK_METHOD(bool, downloadArtifact,
                (const std::string&, const std::string&), (override));

    MOCK_METHOD(bool, backup,
                (const std::string&, const std::string&, const std::string&),
                (override));
};

} // namespace OpenC3::Tests
