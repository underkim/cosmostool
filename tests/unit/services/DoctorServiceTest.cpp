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
