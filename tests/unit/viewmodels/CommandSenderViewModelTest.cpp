// CommandSenderViewModel tests. Same event-loop-driven pattern as the other
// ViewModel test suites in this codebase.
#include "viewmodels/runtime/CommandSenderViewModel.h"

#include "mocks/MockCosmosCommandService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3;
using Tests::MockCosmosCommandService;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace {

void ensureQtApplication()
{
    if (QCoreApplication::instance()) return;

    static int argc = 1;
    static char appName[] = "opencosmos_tests";
    static char* argv[] = { appName, nullptr };
    static QCoreApplication app(argc, argv);
}

class CommandSenderViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();
        vm_ = std::make_unique<ViewModels::CommandSenderViewModel>(commandService_);
    }

    static bool waitFor(QSignalSpy& spy, int timeoutMs = 5000)
    {
        return spy.count() > 0 || spy.wait(timeoutMs);
    }

    NiceMock<MockCosmosCommandService> commandService_;
    std::unique_ptr<ViewModels::CommandSenderViewModel> vm_;
};

TEST_F(CommandSenderViewModelTest, SendGoesThroughTheCommandService)
{
    EXPECT_CALL(commandService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(commandService_, send("INST", "COLLECT", _))
        .WillOnce(Return(Models::CommandSendResult::ok("INST COLLECT with TYPE NORMAL")));

    QSignalSpy spy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    vm_->send("INST", "COLLECT", {{"TYPE", "NORMAL"}});

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(2).toBool());
}

TEST_F(CommandSenderViewModelTest, SendIsRefusedUpFrontWhenApiUnavailable)
{
    EXPECT_CALL(commandService_, isAvailable()).WillOnce(Return(false));
    EXPECT_CALL(commandService_, send(_, _, _)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    vm_->send("INST", "COLLECT", {});

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(2).toBool());
}

TEST_F(CommandSenderViewModelTest, SendWithoutTargetOrCommandIsRefusedUpFront)
{
    EXPECT_CALL(commandService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(commandService_, send(_, _, _)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    vm_->send("", "COLLECT", {});

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(2).toBool());
}

TEST_F(CommandSenderViewModelTest, SendFailureIsNotReportedAsSuccess)
{
    EXPECT_CALL(commandService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(commandService_, send(_, _, _))
        .WillOnce(Return(Models::CommandSendResult::fail("Method not found")));

    QSignalSpy spy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    vm_->send("INST", "BOGUS", {});

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(2).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("Method not found"));
}

TEST_F(CommandSenderViewModelTest, HazardousResultEmitsConfirmationNotCompletion)
{
    EXPECT_CALL(commandService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(commandService_, send("INST", "COLLECT", _))
        .WillOnce(Return(Models::CommandSendResult::hazardousConfirmation(
            "Special collect is destructive")));

    QSignalSpy completedSpy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    QSignalSpy hazardousSpy(vm_.get(), &ViewModels::CommandSenderViewModel::hazardousConfirmationNeeded);
    vm_->send("INST", "COLLECT", {{"TYPE", "SPECIAL"}});

    ASSERT_TRUE(waitFor(hazardousSpy));
    EXPECT_EQ(completedSpy.count(), 0);
    EXPECT_EQ(hazardousSpy.at(0).at(0).toString(), "INST");
    EXPECT_EQ(hazardousSpy.at(0).at(1).toString(), "COLLECT");
    EXPECT_EQ(hazardousSpy.at(0).at(2).toString(), "Special collect is destructive");
}

TEST_F(CommandSenderViewModelTest, SendConfirmedResendsTheExactPendingCommand)
{
    EXPECT_CALL(commandService_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(commandService_, send("INST", "COLLECT", _))
        .WillOnce(Return(Models::CommandSendResult::hazardousConfirmation("Hazardous")));
    EXPECT_CALL(commandService_, sendConfirmed("INST", "COLLECT",
        std::vector<Services::CommandParameter>{{"TYPE", "SPECIAL"}}))
        .WillOnce(Return(Models::CommandSendResult::ok("INST COLLECT with TYPE SPECIAL")));

    QSignalSpy hazardousSpy(vm_.get(), &ViewModels::CommandSenderViewModel::hazardousConfirmationNeeded);
    vm_->send("INST", "COLLECT", {{"TYPE", "SPECIAL"}});
    ASSERT_TRUE(waitFor(hazardousSpy));

    QSignalSpy completedSpy(vm_.get(), &ViewModels::CommandSenderViewModel::commandCompleted);
    vm_->sendConfirmed();

    ASSERT_TRUE(waitFor(completedSpy));
    EXPECT_TRUE(completedSpy.at(0).at(2).toBool());
}

TEST_F(CommandSenderViewModelTest, SendConfirmedWithNoPendingCommandIsANoOp)
{
    EXPECT_CALL(commandService_, sendConfirmed(_, _, _)).Times(0);
    vm_->sendConfirmed();
}

} // namespace
