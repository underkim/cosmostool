#include "services/system/SystemService.h"
#include "mocks/MockCommandExecutor.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using ::testing::Return;
using ::testing::HasSubstr;

class SystemServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor mock_;
    SystemService        sut_{mock_};
};

// `docker ps ... | grep -c cosmos` on zero matches prints "0" but exits 1
// (grep's "no match found" convention) - this mirrors that real shell
// behavior via ExecutorResult::fromProcess, which sets success = (code==0).
TEST_F(SystemServiceTest, IsOpenC3RunningReturnsFalseWhenNoContainersMatch)
{
    EXPECT_CALL(mock_, execute(HasSubstr("grep -c cosmos")))
        .WillOnce(Return(ExecutorResult::fromProcess(1, "0\n", "")));

    EXPECT_FALSE(sut_.isOpenC3Running());
}

TEST_F(SystemServiceTest, IsOpenC3RunningReturnsTrueWhenContainersMatch)
{
    EXPECT_CALL(mock_, execute(HasSubstr("grep -c cosmos")))
        .WillOnce(Return(ExecutorResult::fromProcess(0, "3\n", "")));

    EXPECT_TRUE(sut_.isOpenC3Running());
}

TEST_F(SystemServiceTest, IsOpenC3RunningReturnsFalseWhenExecutorFails)
{
    EXPECT_CALL(mock_, execute(HasSubstr("grep -c cosmos")))
        .WillOnce(Return(ExecutorResult::fail("not connected")));

    EXPECT_FALSE(sut_.isOpenC3Running());
}

TEST_F(SystemServiceTest, GetOpenC3VersionStripsTrailingNewline)
{
    EXPECT_CALL(mock_, execute(HasSubstr("VERSION")))
        .WillOnce(Return(ExecutorResult::ok("6.10.4\n")));

    EXPECT_EQ(sut_.getOpenC3Version(), "6.10.4");
}

TEST_F(SystemServiceTest, GetOpenC3VersionReturnsUnknownOnExecutorFailure)
{
    EXPECT_CALL(mock_, execute(HasSubstr("VERSION")))
        .WillOnce(Return(ExecutorResult::fail("not connected")));

    EXPECT_EQ(sut_.getOpenC3Version(), "unknown");
}
