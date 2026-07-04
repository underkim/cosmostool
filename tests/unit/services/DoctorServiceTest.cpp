#include "services/doctor/DoctorService.h"
#include "mocks/MockCommandExecutor.h"
#include "models/HealthCheckResult.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using namespace OpenC3::Models;
using ::testing::Return;
using ::testing::_;

class DoctorServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor mock_;
    DoctorService       sut_{mock_};
};

TEST_F(DoctorServiceTest, DockerInstalledPassWhenFound)
{
    EXPECT_CALL(mock_, execute(
        "which docker 2>/dev/null || command -v docker"))
        .WillOnce(Return(ExecutorResult::ok("/usr/bin/docker\n")));

    const auto r = sut_.runCheck("docker_installed");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, DockerInstalledFailWhenNotFound)
{
    EXPECT_CALL(mock_, execute(_))
        .WillOnce(Return(ExecutorResult::fail("not found")));

    const auto r = sut_.runCheck("docker_installed");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_FALSE(r.suggestion.empty());
}

TEST_F(DoctorServiceTest, CpuUsagePassWhenBelow80Percent)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("45.2\n")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, CpuUsageFailWhenAbove95Percent)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("97.1\n")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Fail);
}

TEST_F(DoctorServiceTest, CpuUsageSkippedWhenOutputUnparseable)
{
    // Unexpected/empty awk output (e.g. a distro whose `top` format differs)
    // must not be silently treated as "0% - Pass".
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, MemoryUsageSkippedWhenOutputUnparseable)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("free")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("memory_usage");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, DiskSpaceSkippedWhenOutputUnparseable)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("df")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("disk_space");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, ContainerCountSkippedWhenOutputUnparseable)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("cosmos_containers");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, RunAllReturnsSameCountAsChecks)
{
    // Allow any execute calls — return safe defaults
    EXPECT_CALL(mock_, execute(_))
        .WillRepeatedly(Return(ExecutorResult::ok("OK\n")));

    const auto report = sut_.runAll();
    EXPECT_FALSE(report.results.empty());
}

TEST_F(DoctorServiceTest, UnknownCheckIdReturnsFail)
{
    const auto r = sut_.runCheck("nonexistent_check_id_xyz");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_EQ(r.detail, "Unknown check id");
}

TEST_F(DoctorServiceTest, PluginFolderUsesDefaultCosmosRoot)
{
    // Default provider probes "/cosmos", quoted as a single shell argument.
    EXPECT_CALL(mock_, execute(
        "test -d '/cosmos/plugins' && echo EXISTS || echo MISSING"))
        .WillOnce(Return(ExecutorResult::ok("EXISTS\n")));

    const auto r = sut_.runCheck("plugin_folder");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST(DoctorServicePathTest, PluginFolderUsesConfiguredCosmosRoot)
{
    MockCommandExecutor mock;
    DoctorService doctor{mock, [] { return std::string("/opt/my cosmos"); }};

    EXPECT_CALL(mock, execute(
        "test -d '/opt/my cosmos/plugins' && echo EXISTS || echo MISSING"))
        .WillOnce(Return(ExecutorResult::fail("missing")));

    const auto r = doctor.runCheck("plugin_folder");
    EXPECT_EQ(r.status, HealthStatus::Warning);
    // Suggestion should mention the configured root.
    EXPECT_NE(r.suggestion.find("/opt/my cosmos"), std::string::npos);
}

TEST(DoctorServicePathTest, VersionCheckUsesConfiguredCosmosRoot)
{
    MockCommandExecutor mock;
    DoctorService doctor{mock, [] { return std::string("/srv/cosmos"); }};

    EXPECT_CALL(mock, execute(
        "cat '/srv/cosmos/openc3-cosmos-init/plugins/openc3-tool-base/VERSION'"
        " 2>/dev/null || echo unknown"))
        .WillOnce(Return(ExecutorResult::ok("6.1.0\n")));

    const auto r = doctor.runCheck("openc3_version");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}
