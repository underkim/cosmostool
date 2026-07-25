// ScriptViewModel tests.
//
// Mirrors PluginViewModelTest's approach: every action dispatches to a
// worker via QtConcurrent::run and marshals results back with
// QMetaObject::invokeMethod(..., Qt::QueuedConnection), so each test drives
// a real QCoreApplication event loop and waits on the ViewModel's own
// completion signal rather than sleeping.
#include "viewmodels/script/ScriptViewModel.h"

#include "mocks/MockScriptService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3;
using Tests::MockScriptService;
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

class ScriptViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();
        vm_ = std::make_unique<ViewModels::ScriptViewModel>(scriptService_);
    }

    static bool waitFor(QSignalSpy& spy, int timeoutMs = 5000)
    {
        return spy.count() > 0 || spy.wait(timeoutMs);
    }

    NiceMock<MockScriptService> scriptService_;
    std::unique_ptr<ViewModels::ScriptViewModel> vm_;
};

// ── refresh ──────────────────────────────────────────────────────────────────

TEST_F(ScriptViewModelTest, RefreshReadsScriptsFromTheService)
{
    Models::CosmosScript script;
    script.path = "INST/procedures/checkout.rb";

    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(scriptService_, listScripts(_))
        .WillOnce(Return(std::vector<Models::CosmosScript>{script}));

    QSignalSpy spy(vm_.get(), &ViewModels::ScriptViewModel::scriptListChanged);
    vm_->refresh();

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->scriptModel()->rowCount(), 1);
    EXPECT_EQ(vm_->scriptModel()->scriptAt(0)->path, script.path);
}

TEST_F(ScriptViewModelTest, RefreshIsRefusedUpFrontWhenTheApiIsUnavailable)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(false));
    EXPECT_CALL(scriptService_, listScripts(_)).Times(0);

    vm_->refresh();

    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("Not connected"));
}

// ── run ───────────────────────────────────────────────────────────────────────

TEST_F(ScriptViewModelTest, RunGoesThroughTheScriptService)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(scriptService_, run("INST/procedures/checkout.rb"))
        .WillOnce(Return(Models::ScriptRunResult::ok("42")));

    QSignalSpy spy(vm_.get(), &ViewModels::ScriptViewModel::runStarted);
    vm_->run("INST/procedures/checkout.rb");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(ScriptViewModelTest, RunIsRefusedUpFrontWhenTheApiIsUnavailable)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(false));
    EXPECT_CALL(scriptService_, run(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::ScriptViewModel::runStarted);
    vm_->run("INST/procedures/checkout.rb");

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(1).toBool());
}

TEST_F(ScriptViewModelTest, RunFailureIsNotReportedAsSuccess)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(scriptService_, run(_))
        .WillOnce(Return(Models::ScriptRunResult::fail("Forbidden")));

    QSignalSpy spy(vm_.get(), &ViewModels::ScriptViewModel::runStarted);
    vm_->run("INST/procedures/checkout.rb");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(1).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("Forbidden"));
}

TEST_F(ScriptViewModelTest, RunTracksStatusUntilTerminal)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(scriptService_, run(_))
        .WillOnce(Return(Models::ScriptRunResult::ok("42")));

    Models::ScriptExecutionStatus complete;
    complete.querySucceeded = true;
    complete.found = true;
    complete.id = "42";
    complete.state = "completed";
    EXPECT_CALL(scriptService_, status("42")).WillOnce(Return(complete));

    QSignalSpy runSpy(vm_.get(), &ViewModels::ScriptViewModel::runStarted);
    QSignalSpy statusSpy(vm_.get(), &ViewModels::ScriptViewModel::scriptStatusChanged);
    vm_->run("INST/procedures/checkout.rb");

    ASSERT_TRUE(waitFor(runSpy));
    while (statusSpy.count() < 2)
        ASSERT_TRUE(statusSpy.wait(5000));

    const auto terminal = statusSpy.last();
    EXPECT_EQ(terminal.at(0).toString(), "42");
    EXPECT_EQ(terminal.at(1).toString(), "completed");
    EXPECT_TRUE(terminal.at(3).toBool());  // finished
    EXPECT_TRUE(terminal.at(4).toBool());  // successful
}

TEST_F(ScriptViewModelTest, RunTrackingDistinguishesFailureFromSuccess)
{
    EXPECT_CALL(scriptService_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(scriptService_, run(_))
        .WillOnce(Return(Models::ScriptRunResult::ok("42")));

    Models::ScriptExecutionStatus errored;
    errored.querySucceeded = true;
    errored.found = true;
    errored.id = "42";
    errored.state = "crashed";
    EXPECT_CALL(scriptService_, status("42")).WillOnce(Return(errored));

    QSignalSpy runSpy(vm_.get(), &ViewModels::ScriptViewModel::runStarted);
    QSignalSpy statusSpy(vm_.get(), &ViewModels::ScriptViewModel::scriptStatusChanged);
    vm_->run("INST/procedures/checkout.rb");

    ASSERT_TRUE(waitFor(runSpy));
    while (statusSpy.count() < 2)
        ASSERT_TRUE(statusSpy.wait(5000));

    const auto terminal = statusSpy.last();
    EXPECT_TRUE(terminal.at(3).toBool());   // finished
    EXPECT_FALSE(terminal.at(4).toBool());  // NOT successful
}

// ── lifecycle actions require an active run ─────────────────────────────────

TEST_F(ScriptViewModelTest, StopIsANoOpWithoutAnActiveRun)
{
    EXPECT_CALL(scriptService_, stop(_)).Times(0);
    vm_->stop();
    EXPECT_TRUE(vm_->activeRunningScriptId().isEmpty());
}

} // namespace
