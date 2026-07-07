#include "services/docker/DockerService.h"
#include "mocks/MockCommandExecutor.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using ::testing::Return;
using ::testing::HasSubstr;

class DockerServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor   mock_;
    DockerService         sut_{mock_};  // system under test
};

TEST_F(DockerServiceTest, IsDockerRunningReturnsTrueOnSuccess)
{
    EXPECT_CALL(mock_, execute("docker info > /dev/null 2>&1"))
        .WillOnce(Return(ExecutorResult::ok("OK\n")));

    EXPECT_TRUE(sut_.isDockerRunning());
}

TEST_F(DockerServiceTest, IsDockerRunningReturnsFalseOnFailure)
{
    EXPECT_CALL(mock_, execute("docker info > /dev/null 2>&1"))
        .WillOnce(Return(ExecutorResult::fail("daemon not running")));

    EXPECT_FALSE(sut_.isDockerRunning());
}

TEST_F(DockerServiceTest, ListContainersReturnsEmptyOnExecutorFailure)
{
    EXPECT_CALL(mock_, execute(::testing::_))
        .WillOnce(Return(ExecutorResult::fail("error")));

    const auto containers = sut_.listContainers(true);
    EXPECT_TRUE(containers.empty());
}

TEST_F(DockerServiceTest, StartContainerDelegatesToExecutor)
{
    EXPECT_CALL(mock_, execute("docker start 'mycontainer'"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.startContainer("mycontainer"));
}

TEST_F(DockerServiceTest, StopContainerDelegatesToExecutor)
{
    EXPECT_CALL(mock_, execute("docker stop 'mycontainer'"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.stopContainer("mycontainer"));
}

TEST_F(DockerServiceTest, RemoveContainerWithForceFlagPassesFlag)
{
    EXPECT_CALL(mock_, execute("docker rm -f 'mycontainer'"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.removeContainer("mycontainer", true));
}

TEST_F(DockerServiceTest, ContainerNameWithShellMetacharactersIsQuoted)
{
    // A malicious / awkward name must be passed as a single quoted argument so
    // it cannot break out into a separate command.
    EXPECT_CALL(mock_, execute("docker stop '$(rm -rf /); evil'"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.stopContainer("$(rm -rf /); evil"));
}

TEST_F(DockerServiceTest, ComposeDirWithSpacesIsQuoted)
{
    EXPECT_CALL(mock_,
                execute("cd '/cosmos/my plugins' && docker compose up -d"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.composeUp("/cosmos/my plugins"));
}

TEST_F(DockerServiceTest, GetLogsReturnsOutputOnSuccess)
{
    const std::string expected = "2024-01-01 log line\n";
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker logs")))
        .WillOnce(Return(ExecutorResult::ok(expected)));

    const auto logs = sut_.getLogs("mycontainer", 100);
    EXPECT_EQ(logs, expected);
}

TEST_F(DockerServiceTest, GetStatsParsesCpuAndMemory)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker stats")))
        .WillOnce(Return(ExecutorResult::ok(
            R"({"CPUPerc":"12.5%","MemUsage":"2GiB / 4GiB"})")));

    const auto stats = sut_.getStats("mycontainer");
    EXPECT_DOUBLE_EQ(stats.cpuPercent, 12.5);
    EXPECT_DOUBLE_EQ(stats.memUsageMb, 2048.0);  // 2 GiB -> MiB
}

TEST_F(DockerServiceTest, GetStatsHandlesCpuWithoutPercentSign)
{
    // Docker can emit "--" (or other non-percentage text) for a container that
    // is not running. The parser must not throw on a missing '%'.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker stats")))
        .WillOnce(Return(ExecutorResult::ok(
            R"({"CPUPerc":"0","MemUsage":"100MiB / 4GiB"})")));

    const auto stats = sut_.getStats("mycontainer");
    EXPECT_DOUBLE_EQ(stats.cpuPercent, 0.0);
    EXPECT_DOUBLE_EQ(stats.memUsageMb, 100.0);
}

TEST_F(DockerServiceTest, ListComposeServicesParsesEachJsonLine)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker compose ps")))
        .WillOnce(Return(ExecutorResult::ok(
            R"({"Service":"web","Image":"nginx","State":"running"})"
            "\n"
            R"({"Service":"db","Image":"postgres","State":"exited"})"
            "\n")));

    const auto services = sut_.listComposeServices("/cosmos/plugin");
    ASSERT_EQ(services.size(), 2u);
    EXPECT_EQ(services[0].name, "web");
    EXPECT_EQ(services[0].image, "nginx");
    EXPECT_EQ(services[1].name, "db");
}

TEST_F(DockerServiceTest, ListComposeServicesSkipsMalformedLineWithoutDroppingOthers)
{
    // A truncated/corrupted line from `docker compose ps --format json` must
    // not abort parsing of the remaining, well-formed lines.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker compose ps")))
        .WillOnce(Return(ExecutorResult::ok(
            R"({"Service":"web","Image":"nginx","State":"running")"  // truncated, missing closing brace
            "\n"
            R"({"Service":"db","Image":"postgres","State":"exited"})"
            "\n")));

    const auto services = sut_.listComposeServices("/cosmos/plugin");
    ASSERT_EQ(services.size(), 1u);
    EXPECT_EQ(services[0].name, "db");
}
