// InfraViewModel tests. Focused on the QFutureSynchronizer lifetime fix:
// unlike PluginViewModel's single busy_-gated worker_, InfraViewModel's
// slots (loadEnvFile, loadContainers, scaffoldPlugin, ...) have no shared
// busy guard and can legitimately have several QtConcurrent::run workers in
// flight at once - a single QFuture<void> member would not be enough to
// track them all, so pendingWork_ (a QFutureSynchronizer<void>) collects
// every future and the destructor waits on all of them.
#include "viewmodels/infra/InfraViewModel.h"

#include "mocks/MockConnectionService.h"
#include "mocks/MockRemoteFileService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <thread>

using namespace OpenC3;
using Tests::MockConnectionService;
using Tests::MockRemoteFileService;
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

class InfraViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();
        ON_CALL(connection_, onStateChanged(_))
            .WillByDefault(Return(Services::IConnectionService::SubscriptionId{1}));
        ON_CALL(connection_, cosmosRootPath()).WillByDefault(Return(std::string("/cosmos")));
    }

    NiceMock<MockConnectionService>  connection_;
    NiceMock<MockRemoteFileService>  fs_;
};

// The regression test: fire off several independent slots (no shared busy_
// gate between them) and destroy the ViewModel while their workers are
// still deliberately sleeping. Before the fix, pendingWork_ did not exist -
// each QFutureWatcher's deleteLater() neither canceled nor blocked on the
// underlying QtConcurrent task, so several in-flight workers' captured
// `this` and queued callbacks could all outlive the ViewModel at once.
TEST_F(InfraViewModelTest, DestructorWaitsForMultipleInFlightWorkers)
{
    // Drive the connection to "connected" the same way DashboardViewModel/
    // InfraViewModel expect: capture and invoke the onStateChanged
    // callback synchronously (queued invocation still requires the Qt
    // event loop, which QSignalSpy::wait() below pumps).
    std::function<void(const Services::ConnectionEvent&)> stateCallback;
    EXPECT_CALL(connection_, onStateChanged(_))
        .WillOnce([&stateCallback](std::function<void(const Services::ConnectionEvent&)> cb) {
            stateCallback = std::move(cb);
            return Services::IConnectionService::SubscriptionId{1};
        });

    auto vm = std::make_unique<ViewModels::InfraViewModel>(connection_, fs_);

    QSignalSpy connSpy(vm.get(), &ViewModels::InfraViewModel::connectionChanged);
    ASSERT_TRUE(stateCallback);
    stateCallback(Services::ConnectionEvent{Services::ConnectionState::Connected, "", ""});
    ASSERT_TRUE(connSpy.count() > 0 || connSpy.wait(5000));
    ASSERT_TRUE(vm->isConnected());

    // Two independent slots, each backed by a slow mock call - both
    // in-flight at once, exactly the scenario a single QFuture<void>
    // member cannot track.
    EXPECT_CALL(fs_, readFile(_)).WillOnce([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::string("content");
    });
    EXPECT_CALL(fs_, executeCommand(_)).WillOnce([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::string("container1\n");
    });

    vm->loadEnvFile("/cosmos/.env");
    vm->loadContainers();

    // Destroy immediately - both mocks are still "running" on pool threads.
    vm.reset();
    SUCCEED();
}

} // namespace
