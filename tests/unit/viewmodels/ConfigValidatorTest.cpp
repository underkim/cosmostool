#include "viewmodels/validation/ConfigValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

TEST(ConfigValidatorTest, ClassifiesByContent)
{
    EXPECT_EQ(ConfigValidator::classify("x.txt", "SCREEN AUTO AUTO 1.0\n"),
              ConfigValidator::FileKind::Screen);
    EXPECT_EQ(ConfigValidator::classify("x.txt", "COMMAND TGT NOOP BIG_ENDIAN \"x\"\n"),
              ConfigValidator::FileKind::CmdTlm);
    EXPECT_EQ(ConfigValidator::classify("x.txt", "TELEMETRY TGT HK BIG_ENDIAN \"x\"\n"),
              ConfigValidator::FileKind::CmdTlm);
}

TEST(ConfigValidatorTest, ClassifiesPluginByName)
{
    EXPECT_EQ(ConfigValidator::classify("/p/plugin.txt", "VARIABLE port 8080\n"),
              ConfigValidator::FileKind::PluginConfig);
}

TEST(ConfigValidatorTest, ValidateContentDispatchesToScreen)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "VERTICAL\n"
        "  LABEL \"x\"\n";  // missing END

    const auto report = ConfigValidator::validateContent(
        ConfigValidator::FileKind::Screen, src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(ConfigValidatorTest, ReportSummaryCountsSeverities)
{
    ValidationReport report;
    report.add(Diagnostic::error(1, "e"));
    report.add(Diagnostic::warning(2, "w"));
    EXPECT_EQ(report.errorCount(), 1);
    EXPECT_EQ(report.warningCount(), 1);
    EXPECT_FALSE(report.ok());
}
