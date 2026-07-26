// PacketToolsViewModel tests. Focused on the lifetime fix: refreshLogList/
// openLogFile/analyzePackets each dispatch a QtConcurrent::run worker
// capturing `this` raw; before the fix the future was discarded
// ((void)QtConcurrent::run(...)) and never waited on in the destructor.
#include "viewmodels/packettools/PacketToolsViewModel.h"

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

class PacketToolsViewModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensureQtApplication();
        ON_CALL(connection_, onStateChanged(_))
            .WillByDefault(Return(Services::IConnectionService::SubscriptionId{1}));
    }

    NiceMock<MockConnectionService>  connection_;
    NiceMock<MockRemoteFileService>  fs_;
};

TEST_F(PacketToolsViewModelTest, RefreshLogListReadsFromTheService)
{
    EXPECT_CALL(fs_, listDirectory(_))
        .WillOnce(Return(std::vector<std::string>{"tlm.bin"}));

    auto vm = std::make_unique<ViewModels::PacketToolsViewModel>(connection_, fs_);
    QSignalSpy spy(vm.get(), &ViewModels::PacketToolsViewModel::logListReady);
    vm->refreshLogList();

    ASSERT_TRUE(spy.count() > 0 || spy.wait(5000));
    ASSERT_EQ(spy.at(0).at(0).toStringList().size(), 1);
}

// The regression test: destroy while refreshLogList()'s worker is still
// deliberately sleeping. Before the fix (fire-and-forget QtConcurrent::run,
// no stored future, no destructor wait) this could crash or corrupt memory
// once the worker's queued callback ran against a freed ViewModel.
TEST_F(PacketToolsViewModelTest, DestructorWaitsForAnInFlightRefresh)
{
    EXPECT_CALL(fs_, listDirectory(_)).WillOnce([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::vector<std::string>{};
    });

    auto vm = std::make_unique<ViewModels::PacketToolsViewModel>(connection_, fs_);
    vm->refreshLogList();
    vm.reset();
    SUCCEED();
}

TEST_F(PacketToolsViewModelTest, RepeatedConstructDestroyUnderLoadIsSafe)
{
    for (int i = 0; i < 5; ++i) {
        EXPECT_CALL(fs_, listDirectory(_)).WillOnce([](const std::string&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return std::vector<std::string>{};
        });

        auto vm = std::make_unique<ViewModels::PacketToolsViewModel>(connection_, fs_);
        vm->refreshLogList();
        vm.reset();
    }
    SUCCEED();
}

} // namespace
