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
    EXPECT_CALL(mock_, execute("docker start mycontainer"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.startContainer("mycontainer"));
}

TEST_F(DockerServiceTest, StopContainerDelegatesToExecutor)
{
    EXPECT_CALL(mock_, execute("docker stop mycontainer"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.stopContainer("mycontainer"));
}

TEST_F(DockerServiceTest, RemoveContainerWithForceFlagPassesFlag)
{
    EXPECT_CALL(mock_, execute("docker rm -f mycontainer"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.removeContainer("mycontainer", true));
}

TEST_F(DockerServiceTest, GetLogsReturnsOutputOnSuccess)
{
    const std::string expected = "2024-01-01 log line\n";
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker logs")))
        .WillOnce(Return(ExecutorResult::ok(expected)));

    const auto logs = sut_.getLogs("mycontainer", 100);
    EXPECT_EQ(logs, expected);
}
