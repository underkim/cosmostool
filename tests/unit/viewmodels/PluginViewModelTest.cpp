// PluginViewModel tests.
//
// The ViewModel layer had no tests at all, which is exactly where this
// codebase's only genuine threading risk lives: every action dispatches to a
// worker via QtConcurrent::run and marshals results back with
// QMetaObject::invokeMethod(..., Qt::QueuedConnection). Those queued calls
// only run while an event loop is spinning, so each test below drives one and
// waits on the ViewModel's own completion signal rather than sleeping.
//
// The behaviour under test is the step-2/3 rework: install and remove must go
// through the COSMOS API (ICosmosPluginService), while listing and building
// stay on the shell (IWorkspaceService).
#include "viewmodels/plugin/PluginViewModel.h"

#include "mocks/MockConnectionService.h"
#include "mocks/MockCosmosPluginService.h"
#include "mocks/MockWorkspaceService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3;
using Services::PluginOperationResult;
using Tests::MockConnectionService;
using Tests::MockCosmosPluginService;
using Tests::MockWorkspaceService;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace {

/// Matches the pattern PacketSimulatorTest established: a QObject-based
/// ViewModel needs a QCoreApplication to exist first, and the queued
/// invocations these tests rely on need its event loop. Guarded because the
/// whole suite links into one binary and another fixture may have created it.
void ensureQtApplication()
{
    if (QCoreApplication::instance()) return;

    static int argc = 1;
    static char appName[] = "opencosmos_tests";
    static char* argv[] = { appName, nullptr };
    static QCoreApplication app(argc, argv);
}

class PluginViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();

        // Every path calls cosmosRootPath() to resolve the plugins directory.
        ON_CALL(connection_, cosmosRootPath())
            .WillByDefault(Return(std::string("/cosmos")));
        ON_CALL(connection_, activeProfile())
            .WillByDefault(Return(nullptr));

        vm_ = std::make_unique<ViewModels::PluginViewModel>(
            workspace_, cosmos_, connection_);
    }

    /// Spins the event loop until `spy` receives a signal or the deadline
    /// passes. Necessary because results are delivered by a queued invocation
    /// that cannot run unless the loop is processing events.
    static bool waitFor(QSignalSpy& spy, int timeoutMs = 5000)
    {
        return spy.count() > 0 || spy.wait(timeoutMs);
    }

    NiceMock<MockWorkspaceService>    workspace_;
    NiceMock<MockCosmosPluginService> cosmos_;
    NiceMock<MockConnectionService>   connection_;

    std::unique_ptr<ViewModels::PluginViewModel> vm_;
};

// ── refresh: shell transport ─────────────────────────────────────────────────

TEST_F(PluginViewModelTest, RefreshReadsSourcesFromTheWorkspaceService)
{
    // Listing must NOT go to the API: this pane shows plugin source folders on
    // the host, which is a different question from "what is installed".
    Models::Plugin plugin;
    plugin.name    = "openc3-cosmos-demo";
    plugin.version = "1.0.0";

    EXPECT_CALL(workspace_, listSources("/cosmos"))
        .WillOnce(Return(std::vector<Models::Plugin>{plugin}));
    EXPECT_CALL(cosmos_, listInstalled()).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::pluginListChanged);
    vm_->refresh();

    ASSERT_TRUE(waitFor(spy));
    EXPECT_EQ(vm_->pluginModel()->rowCount(), 1);
}

TEST_F(PluginViewModelTest, RefreshInstalledReadsInstancesFromTheCosmosApi)
{
    Models::Plugin plugin;
    plugin.name    = "openc3-cosmos-demo-1.0.0.gem__0";
    plugin.version = "openc3-cosmos-demo-1.0.0.gem";

    EXPECT_CALL(cosmos_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(cosmos_, listInstalled())
        .WillOnce(Return(std::vector<Models::Plugin>{plugin}));
    EXPECT_CALL(workspace_, listSources(_)).Times(0);

    QSignalSpy spy(vm_.get(),
                   &ViewModels::PluginViewModel::installedPluginListChanged);
    vm_->refreshInstalled();

    ASSERT_TRUE(waitFor(spy));
    ASSERT_EQ(vm_->installedPluginModel()->rowCount(), 1);
    EXPECT_EQ(vm_->installedPluginModel()->pluginAt(0)->name, plugin.name);
}

// ── install: API transport ───────────────────────────────────────────────────

TEST_F(PluginViewModelTest, InstallGoesThroughTheCosmosApiNotTheShell)
{
    // The regression this guards: install used to copy a .gem into
    // <root>/plugins/ over the shell and report success, with no effect on a
    // running COSMOS.
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, installGem("/tmp/demo.gem"))
        .WillOnce(Return(PluginOperationResult::ok("COSMOS started installing demo.")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->install("/tmp/demo.gem");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(PluginViewModelTest, InstallIsRefusedUpFrontWhenTheApiIsUnavailable)
{
    // Without an API endpoint the operation cannot possibly work, so it must
    // fail immediately with an explanation rather than dispatching a worker
    // that will fail obscurely.
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(false));
    EXPECT_CALL(cosmos_, installGem(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->install("/tmp/demo.gem");

    // Emitted synchronously on the refusal path - no event loop needed.
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(1).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("API"));
}

TEST_F(PluginViewModelTest, InstallFailureSurfacesTheCosmosMessage)
{
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, installGem(_))
        .WillOnce(Return(PluginOperationResult::fail("Gem file is empty")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->install("/tmp/empty.gem");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(1).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("Gem file is empty"));
}

TEST_F(PluginViewModelTest, InstallReportsJobStartedRatherThanCompleted)
{
    // COSMOS spawns openc3cli and returns a process name without waiting, so
    // the UI must not claim the plugin is installed.
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, installGem(_))
        .WillOnce(Return(PluginOperationResult::ok(
            "COSMOS started installing demo. It may take a moment to appear.",
            "plugin_install_1")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->install("/tmp/demo.gem");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("started"));
}

TEST_F(PluginViewModelTest, BuildAndInstallDownloadsRemoteGemBeforeApiUpload)
{
    const std::string remote =
        "/cosmos/plugins/demo/openc3-cosmos-demo-1.0.0.gem";
    EXPECT_CALL(cosmos_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(workspace_, buildArtifact("/cosmos/plugins/demo"))
        .WillOnce(Return(Services::PluginBuildResult::ok(
            remote, "Successfully built RubyGem")));
    EXPECT_CALL(workspace_, downloadArtifact(remote, ::testing::HasSubstr(
        "openc3-cosmos-demo-1.0.0.gem")))
        .WillOnce(Return(true));
    EXPECT_CALL(cosmos_, installGem(::testing::HasSubstr(
        "openc3-cosmos-demo-1.0.0.gem")))
        .WillOnce(Return(PluginOperationResult::ok(
            "COSMOS started installing demo.")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->buildAndInstall("/cosmos/plugins/demo");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(PluginViewModelTest, BuildAndInstallStopsWhenArtifactDownloadFails)
{
    const std::string remote = "/remote/demo.gem";
    EXPECT_CALL(cosmos_, isAvailable()).WillOnce(Return(true));
    EXPECT_CALL(workspace_, buildArtifact(_))
        .WillOnce(Return(Services::PluginBuildResult::ok(remote)));
    EXPECT_CALL(workspace_, downloadArtifact(remote, _)).WillOnce(Return(false));
    EXPECT_CALL(cosmos_, installGem(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->buildAndInstall("/cosmos/plugins/demo");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(1).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("could not be downloaded"));
}

// ── remove: API transport ────────────────────────────────────────────────────

TEST_F(PluginViewModelTest, RemoveGoesThroughTheCosmosApi)
{
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, uninstall("demo.gem__1"))
        .WillOnce(Return(PluginOperationResult::ok("COSMOS started removing demo.")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->remove("demo.gem__1");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(PluginViewModelTest, RemoveRejectsAWorkspaceSourceName)
{
    // PluginModel#create always assigns __<counter> to an installed instance.
    // A Workspace row lacks that identity and cannot safely select between
    // multiple installed instances of the same gem.
    EXPECT_CALL(cosmos_, isAvailable()).Times(0);
    EXPECT_CALL(cosmos_, uninstall(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->remove("openc3-cosmos-demo");

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(1).toBool());
    EXPECT_THAT(vm_->statusMessage().toStdString(),
                ::testing::HasSubstr("source folder"));
}

TEST_F(PluginViewModelTest, RemoveIsRefusedUpFrontWhenTheApiIsUnavailable)
{
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(false));
    EXPECT_CALL(cosmos_, uninstall(_)).Times(0);

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->remove("demo.gem__1");

    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(1).toBool());
}

TEST_F(PluginViewModelTest, RemoveFailureIsNotReportedAsSuccess)
{
    // The old shell implementation used `rm -f`, which exits 0 whether or not
    // the file existed, so a mistyped name looked like a successful removal.
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, uninstall(_))
        .WillOnce(Return(PluginOperationResult::fail("Plugin not found")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->remove("nope__1");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(1).toBool());
}

// ── validate / build: shell transport ────────────────────────────────────────

TEST_F(PluginViewModelTest, ValidateUsesTheWorkspaceService)
{
    Models::PluginValidationResult result;
    result.valid = true;

    EXPECT_CALL(workspace_, validate("/cosmos/plugins/demo"))
        .WillOnce(Return(result));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::validationComplete);
    vm_->validate("/cosmos/plugins/demo");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(0).toBool());
}

TEST_F(PluginViewModelTest, ValidationIssuesAreJoinedIntoTheSummary)
{
    Models::PluginValidationResult result;
    result.valid = false;
    Models::PluginValidationIssue issue;
    issue.code    = "MISSING_PLUGIN_TXT";
    issue.message = "plugin.txt is missing.";
    result.issues.push_back(issue);

    EXPECT_CALL(workspace_, validate(_)).WillOnce(Return(result));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::validationComplete);
    vm_->validate("/cosmos/plugins/broken");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(0).toBool());
    EXPECT_THAT(spy.at(0).at(1).toString().toStdString(),
                ::testing::HasSubstr("MISSING_PLUGIN_TXT"));
}

TEST_F(PluginViewModelTest, BuildStaysOnTheShellTransport)
{
    // Building a gem is a local Ruby toolchain operation, not a COSMOS one.
    EXPECT_CALL(workspace_, build("/cosmos/plugins/demo"))
        .WillOnce(Return(std::string("Successfully built RubyGem\n./demo.gem\n")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->build("/cosmos/plugins/demo");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_TRUE(spy.at(0).at(1).toBool());
}

TEST_F(PluginViewModelTest, BuildFailureIsDetectedFromErrorText)
{
    // build() has no success flag - failure is signalled by "ERROR" in the
    // output, which is why WorkspaceService::build guarantees that keyword.
    EXPECT_CALL(workspace_, build(_))
        .WillOnce(Return(std::string(
            "ERROR: No .gemspec file found in the plugin folder.")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->build("/cosmos/plugins/broken");

    ASSERT_TRUE(waitFor(spy));
    EXPECT_FALSE(spy.at(0).at(1).toBool());
}

// ── busy guard ───────────────────────────────────────────────────────────────

TEST_F(PluginViewModelTest, ConcurrentActionsAreRejectedWhileBusy)
{
    // Guards against double-clicking Install firing two overlapping jobs.
    EXPECT_CALL(cosmos_, isAvailable()).WillRepeatedly(Return(true));
    EXPECT_CALL(cosmos_, installGem(_))
        .Times(1)
        .WillOnce(Return(PluginOperationResult::ok("started")));

    QSignalSpy spy(vm_.get(), &ViewModels::PluginViewModel::actionCompleted);
    vm_->install("/tmp/a.gem");
    vm_->install("/tmp/b.gem");   // must be dropped while the first is in flight

    ASSERT_TRUE(waitFor(spy));
}

} // namespace
