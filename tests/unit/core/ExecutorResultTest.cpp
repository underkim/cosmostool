#include "core/connection/ExecutorResult.h"
#include <gtest/gtest.h>

using OpenC3::Core::Connection::ExecutorResult;

TEST(ExecutorResult, OkFactorySetsTrueAndOutput)
{
    auto r = ExecutorResult::ok("hello", 0);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.exitCode, 0);
    EXPECT_EQ(r.stdOut, "hello");
    EXPECT_TRUE(r.errorMessage.empty());
}

TEST(ExecutorResult, FailFactorySetsFalseAndError)
{
    auto r = ExecutorResult::fail("connection refused", 1);
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.exitCode, 1);
    EXPECT_EQ(r.errorMessage, "connection refused");
}

TEST(ExecutorResult, FromProcessZeroExitIsSuccess)
{
    auto r = ExecutorResult::fromProcess(0, "output", "");
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.stdOut, "output");
}

TEST(ExecutorResult, FromProcessNonZeroExitIsFail)
{
    auto r = ExecutorResult::fromProcess(127, "", "command not found");
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.exitCode, 127);
    EXPECT_EQ(r.errorMessage, "command not found");
}

TEST(ExecutorResult, BoolOperatorReflectsSuccess)
{
    EXPECT_TRUE(static_cast<bool>(ExecutorResult::ok()));
    EXPECT_FALSE(static_cast<bool>(ExecutorResult::fail("err")));
}
