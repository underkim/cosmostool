#include "viewmodels/validator/ValidatorViewModel.h"

#include <gtest/gtest.h>

using namespace OpenC3::ViewModels;

TEST(ValidatorViewModelTest, EmptyValidatorIdAutoDetectsAndDoesNotReportUnknownId)
{
    ValidatorViewModel vm;
    vm.validateTextWith(QString(), QStringLiteral("SCREEN AUTO AUTO 1.0"));
    for (const auto& d : vm.report().diagnostics)
        EXPECT_FALSE(d.message.contains("Unknown validator id"));
}

TEST(ValidatorViewModelTest, KnownValidatorIdRunsThatValidator)
{
    ValidatorViewModel vm;
    vm.validateTextWith(QStringLiteral("screen"), QStringLiteral("SCREEN AUTO AUTO 1.0"));
    for (const auto& d : vm.report().diagnostics)
        EXPECT_FALSE(d.message.contains("Unknown validator id"));
}

// An unrecognized validator id (e.g. a stale id from a renamed/removed
// validator) must surface as a visible error, not silently report zero
// diagnostics - an empty report reads as "no issues found", which is a false
// pass rather than the wiring failure it actually is.
TEST(ValidatorViewModelTest, UnknownValidatorIdReportsAnErrorInsteadOfAFalsePass)
{
    ValidatorViewModel vm;
    vm.validateTextWith(QStringLiteral("not-a-real-id"), QStringLiteral("irrelevant content"));

    ASSERT_FALSE(vm.report().diagnostics.isEmpty());
    EXPECT_GT(vm.report().errorCount(), 0);
    EXPECT_TRUE(vm.report().diagnostics.first().message.contains("not-a-real-id"));
}
