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

TEST_F(SystemServiceTest, GetMetricsParsesCpuAndDiskFields)
{
    EXPECT_CALL(mock_, execute(HasSubstr("top -bn1")))
        .WillOnce(Return(ExecutorResult::ok("12.5\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("free -m")))
        .WillOnce(Return(ExecutorResult::ok("2000 500 1500\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("df -BG")))
        .WillOnce(Return(ExecutorResult::ok("100G 40G 60G 40%\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("hostname")))
        .WillOnce(Return(ExecutorResult::ok("myhost\n")));

    const auto metrics = sut_.getMetrics();
    EXPECT_DOUBLE_EQ(metrics.cpuPercent, 12.5);
    ASSERT_EQ(metrics.disks.size(), 1u);
    EXPECT_DOUBLE_EQ(metrics.disks[0].totalGb, 100.0);
    EXPECT_DOUBLE_EQ(metrics.disks[0].usedGb, 40.0);
    EXPECT_DOUBLE_EQ(metrics.disks[0].availableGb, 60.0);
    EXPECT_DOUBLE_EQ(metrics.disks[0].usedPercent, 40.0);
    EXPECT_EQ(metrics.hostname, "myhost");
}

// A malformed line from `top`/`df` (format varies across distros/locales)
// must not throw or abort the rest of getMetrics() - it should just leave
// that one field at its zero default for this polling tick.
TEST_F(SystemServiceTest, GetMetricsLeavesFieldsAtDefaultOnMalformedOutput)
{
    EXPECT_CALL(mock_, execute(HasSubstr("top -bn1")))
        .WillOnce(Return(ExecutorResult::ok("not-a-number\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("free -m")))
        .WillOnce(Return(ExecutorResult::ok("2000 500 1500\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("df -BG")))
        .WillOnce(Return(ExecutorResult::ok("garbage garbage garbage garbage\n")));
    EXPECT_CALL(mock_, execute(HasSubstr("hostname")))
        .WillOnce(Return(ExecutorResult::ok("myhost\n")));

    const auto metrics = sut_.getMetrics();
    EXPECT_DOUBLE_EQ(metrics.cpuPercent, 0.0);
    ASSERT_EQ(metrics.disks.size(), 1u);
    EXPECT_DOUBLE_EQ(metrics.disks[0].totalGb, 0.0);
}
