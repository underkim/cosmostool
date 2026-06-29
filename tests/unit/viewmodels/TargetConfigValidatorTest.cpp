#include "viewmodels/validation/TargetConfigValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

TEST(TargetConfigValidatorTest, ValidTargetFileHasNoFindings)
{
    const QString src =
        "LANGUAGE python\n"
        "REQUIRE limits_response.rb\n"
        "IGNORE_PARAMETER CCSDSVER\n"
        "TLM_LOG_CYCLE_TIME 3600\n";

    const auto report = TargetConfigValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_EQ(report.warningCount(), 0);
}

TEST(TargetConfigValidatorTest, UnknownKeywordWarns)
{
    const QString src = "BOGUS_KEYWORD foo\n";

    const auto report = TargetConfigValidator{}.validate(src);
    EXPECT_GE(report.warningCount(), 1);
}

TEST(TargetConfigValidatorTest, MissingArgumentWarns)
{
    const QString src = "REQUIRE\n";

    const auto report = TargetConfigValidator{}.validate(src);
    EXPECT_GE(report.warningCount(), 1);
}

TEST(TargetConfigValidatorTest, BadLanguageWarns)
{
    const QString src = "LANGUAGE cobol\n";

    const auto report = TargetConfigValidator{}.validate(src);
    EXPECT_GE(report.warningCount(), 1);
}

TEST(TargetConfigValidatorTest, AppliesToTargetTxt)
{
    EXPECT_TRUE(TargetConfigValidator{}.appliesTo(
        QStringLiteral("/x/targets/INST/target.txt"), QString()));
    EXPECT_FALSE(TargetConfigValidator{}.appliesTo(
        QStringLiteral("/x/plugin.txt"), QString()));
}
