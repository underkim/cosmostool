#include "core/connection/ExecutorProxy.h"
#include "mocks/MockCommandExecutor.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Core::Connection;
using namespace OpenC3::Tests;
using ::testing::Return;

class ExecutorProxyTest : public ::testing::Test {
protected:
    ExecutorProxy       proxy_;
    MockCommandExecutor real_;
};

// Before any real executor is swapped in, the proxy must behave like the
// null object: connected == false and lastError() == "" (not a crash from
// dereferencing a null current_ pointer).
TEST_F(ExecutorProxyTest, DefaultsToNullExecutorBehavior)
{
    EXPECT_FALSE(proxy_.isConnected());
    EXPECT_EQ(proxy_.lastError(), "");
    EXPECT_FALSE(static_cast<bool>(proxy_.execute("echo hi")));
}

TEST_F(ExecutorProxyTest, SwapForwardsCallsToRealExecutor)
{
    EXPECT_CALL(real_, isConnected()).WillOnce(Return(true));
    proxy_.swap(&real_);
    EXPECT_TRUE(proxy_.isConnected());
}

// ExecutorProxy's class comment says it "atomically delegates all calls to
// the current real executor" - lastError() was previously missing from that
// forwarding (it silently used ICommandExecutor's default "" body instead of
// reaching the real executor), which would have hidden the actual connect
// failure reason from any caller that read it through the proxy.
TEST_F(ExecutorProxyTest, LastErrorForwardsToRealExecutor)
{
    EXPECT_CALL(real_, lastError()).WillOnce(Return("auth failed"));
    proxy_.swap(&real_);
    EXPECT_EQ(proxy_.lastError(), "auth failed");
}

TEST_F(ExecutorProxyTest, ResetRevertsToNullExecutor)
{
    EXPECT_CALL(real_, isConnected()).WillOnce(Return(true));
    proxy_.swap(&real_);
    EXPECT_TRUE(proxy_.isConnected());

    proxy_.reset();
    EXPECT_FALSE(proxy_.isConnected());
    EXPECT_EQ(proxy_.lastError(), "");
}

TEST_F(ExecutorProxyTest, SwapWithNullptrActsLikeReset)
{
    EXPECT_CALL(real_, isConnected()).WillOnce(Return(true));
    proxy_.swap(&real_);
    EXPECT_TRUE(proxy_.isConnected());

    proxy_.swap(nullptr);
    EXPECT_FALSE(proxy_.isConnected());
}
