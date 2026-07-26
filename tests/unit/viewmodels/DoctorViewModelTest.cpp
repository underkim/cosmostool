// DoctorViewModel tests. Focused specifically on the lifetime fix: before
// it, runAllChecks()'s QtConcurrent::run worker captured `this` raw and was
// never waited on in the destructor - only the QFutureWatcher's
// deleteLater() ran, which neither cancels nor blocks on the underlying
// task. Destroying the ViewModel while a check was still running left the
// worker's queued callback dereferencing freed memory.
#include "viewmodels/doctor/DoctorViewModel.h"

#include "mocks/MockDoctorService.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <thread>

using namespace OpenC3;
using Tests::MockDoctorService;
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

class DoctorViewModelTest : public ::testing::Test {
protected:
    void SetUp() override { ensureQtApplication(); }

    NiceMock<MockDoctorService> doctor_;
};

TEST_F(DoctorViewModelTest, RunAllChecksPopulatesResultsModel)
{
    Models::HealthCheckResult result;
    result.checkId = "docker_installed";
    result.name = "Docker Installed";
    result.category = "Docker";
    result.status = Models::HealthStatus::Pass;

    // DoctorViewModel builds its results vector entirely from the
    // onProgress callback (see runAllChecks()), not from runAll()'s return
    // value - the mock must actually invoke it, matching what
    // DoctorService::runAll() does for each check as it completes.
    EXPECT_CALL(doctor_, runAll(_))
        .WillOnce([&result](std::function<void(const Models::HealthCheckResult&)> onProgress) {
            onProgress(result);
            return Models::DoctorReport{{result}};
        });

    auto vm = std::make_unique<ViewModels::DoctorViewModel>(doctor_);
    QSignalSpy spy(vm.get(), &ViewModels::DoctorViewModel::summaryChanged);
    vm->runAllChecks();

    ASSERT_TRUE(spy.count() > 0 || spy.wait(5000));
    EXPECT_EQ(vm->resultsModel()->rowCount(), 1);
    EXPECT_EQ(vm->passCount(), 1);
}

// The actual regression test: destroy the ViewModel while runAllChecks()'s
// worker is still (deliberately, via a sleep) in flight. Before the fix,
// this reliably crashed or corrupted memory under ASAN/valgrind because the
// worker thread's captured `this` and its queued invokeMethod() callback
// both outlived the ViewModel; the destructor now blocks until the future
// completes, so this must return cleanly every time.
TEST_F(DoctorViewModelTest, DestructorWaitsForAnInFlightCheckRun)
{
    EXPECT_CALL(doctor_, runAll(_)).WillOnce([](auto) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return Models::DoctorReport{};
    });

    auto vm = std::make_unique<ViewModels::DoctorViewModel>(doctor_);
    vm->runAllChecks();
    // Destroy immediately - the mock is still "running" on the pool thread.
    vm.reset();
    // Reaching here without a crash/UB is the assertion; gtest's own
    // process exit code catches a real crash if the fix regresses.
    SUCCEED();
}

TEST_F(DoctorViewModelTest, RepeatedConstructDestroyUnderLoadIsSafe)
{
    // Mirrors PluginViewModelTest's repeated-construct-destroy pattern -
    // exercises the same worker-vs-destructor race several times in one
    // run to make a reintroduced fire-and-forget bug more likely to crash
    // this test rather than pass by luck.
    for (int i = 0; i < 5; ++i) {
        EXPECT_CALL(doctor_, runAll(_)).WillOnce([](auto) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return Models::DoctorReport{};
        });

        auto vm = std::make_unique<ViewModels::DoctorViewModel>(doctor_);
        vm->runAllChecks();
        vm.reset();
    }
    SUCCEED();
}

} // namespace
