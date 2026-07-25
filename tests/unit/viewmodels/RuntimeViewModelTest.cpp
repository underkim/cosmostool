// RuntimeViewModel tests. Same event-loop-driven pattern as
// PluginViewModelTest/ScriptViewModelTest: every action dispatches via
// QtConcurrent::run and marshals its result back with a queued invocation,
// so tests drive a real QCoreApplication event loop and wait on the
// ViewModel's own completion signal.
#include "viewmodels/runtime/RuntimeViewModel.h"

#include "mocks/MockCosmosRuntimeService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3;
using Tests::MockCosmosRuntimeService;
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

class RuntimeViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();
        vm_ = std::make_unique<ViewModels::RuntimeViewModel>(runtime_);
    }

    static bool waitFor(QSignalSpy& spy, int timeoutMs = 5000)
    {
        return spy.count() > 0 || spy.wait(timeoutMs);
    }

    NiceMock<MockCosmosRuntimeService> runtime_;
    std::unique_ptr<ViewModels::RuntimeViewModel> vm_;
};

// ── refreshTargets ───────────────────────────────────────────────────────────

TEST_F(RuntimeViewModelTest, RefreshTargetsReadsFromTheService)
{
    Models::RuntimeTarget target;
    target.name = "INST";

    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(runtime_, listTargets())
        .WillOnce(Return(std::vector<Models::RuntimeTarget>{target}));

    QSignalSpy spy(vm_.get(), &ViewModels::RuntimeViewModel::targetListChanged);
    vm_->refreshTargets();

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->targetModel()->rowCount(), 1);
    EXPECT_EQ(vm_->targetModel()->targetAt(0)->name, "INST");
}

TEST_F(RuntimeViewModelTest, RefreshTargetsIsRefusedWhenApiUnavailable)
{
    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(false));
    EXPECT_CALL(runtime_, listTargets()).Times(0);

    vm_->refreshTargets();

    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("Not connected"));
}

// ── loadCommands / loadTelemetry ─────────────────────────────────────────────

TEST_F(RuntimeViewModelTest, LoadCommandsUsesTheCommandListingCall)
{
    Models::RuntimePacket packet;
    packet.targetName = "INST";
    packet.packetName = "COLLECT";

    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(runtime_, listCommands("INST"))
        .WillOnce(Return(std::vector<Models::RuntimePacket>{packet}));
    EXPECT_CALL(runtime_, listTelemetry(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::RuntimeViewModel::packetListChanged);
    vm_->loadCommands("INST");

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->packetModel()->rowCount(), 1);
    EXPECT_EQ(vm_->packetModel()->packetAt(0)->packetName, "COLLECT");
}

TEST_F(RuntimeViewModelTest, LoadTelemetryUsesTheTelemetryListingCall)
{
    Models::RuntimePacket packet;
    packet.targetName = "INST";
    packet.packetName = "HEALTH_STATUS";

    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(runtime_, listTelemetry("INST"))
        .WillOnce(Return(std::vector<Models::RuntimePacket>{packet}));
    EXPECT_CALL(runtime_, listCommands(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::RuntimeViewModel::packetListChanged);
    vm_->loadTelemetry("INST");

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->packetModel()->rowCount(), 1);
    EXPECT_EQ(vm_->packetModel()->packetAt(0)->packetName, "HEALTH_STATUS");
}

// ── refreshTelemetryValues / polling ─────────────────────────────────────────

TEST_F(RuntimeViewModelTest, RefreshTelemetryValuesReadsCurrentValues)
{
    Models::TelemetryItemValue value;
    value.itemName = "TEMP1";
    value.value = "25.5";
    value.limitsState = "GREEN";

    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(runtime_, telemetryValues("INST", "HEALTH_STATUS"))
        .WillOnce(Return(std::vector<Models::TelemetryItemValue>{value}));

    QSignalSpy spy(vm_.get(), &ViewModels::RuntimeViewModel::telemetryValuesChanged);
    vm_->refreshTelemetryValues("INST", "HEALTH_STATUS");

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->telemetryModel()->rowCount(), 1);
}

TEST_F(RuntimeViewModelTest, SetTelemetryPollingWithEmptyArgsStopsWithoutCalling)
{
    EXPECT_CALL(runtime_, telemetryValues(_, _)).Times(0);
    vm_->setTelemetryPolling("", "");
    // No crash, no call - polling was never started.
}

TEST_F(RuntimeViewModelTest, SetTelemetryPollingTriggersAnImmediateRefresh)
{
    EXPECT_CALL(runtime_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(runtime_, telemetryValues("INST", "HEALTH_STATUS"))
        .WillOnce(Return(std::vector<Models::TelemetryItemValue>{}));

    QSignalSpy spy(vm_.get(), &ViewModels::RuntimeViewModel::telemetryValuesChanged);
    vm_->setTelemetryPolling("INST", "HEALTH_STATUS");

    ASSERT_TRUE(waitFor(spy));

    // Stop polling before the fixture tears down so a stray QTimer tick
    // doesn't race the mock's lifetime.
    vm_->setTelemetryPolling("", "");
}

} // namespace
