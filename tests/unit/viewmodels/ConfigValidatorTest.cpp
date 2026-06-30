#include "viewmodels/validation/ConfigValidator.h"

#include <gtest/gtest.h>
#include <QString>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace OpenC3::ViewModels::Validation;

namespace {

// Returns true if any diagnostic in the report carries the given rule code.
bool hasRule(const ValidationReport& report, const QString& rule)
{
    for (const auto& d : report.diagnostics)
        if (d.rule == rule)
            return true;
    return false;
}

// Write text into <dir>/<relative>, creating parent directories as needed.
void writeFile(const QString& dir, const QString& relative, const QString& text)
{
    const QString full = dir + QStringLiteral("/") + relative;
    QDir().mkpath(QFileInfo(full).absolutePath());
    QFile file(full);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << text;
}

} // namespace

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

TEST(ConfigValidatorTest, ValidateFolderReportsEmptyWhenNoConfigs)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "notes.txt", "just some unrelated text\n");

    const auto report = ConfigValidator::validateFolder(dir.path());
    EXPECT_TRUE(hasRule(report, "folder.empty"));
    EXPECT_FALSE(hasRule(report, "folder.clean"));
}

TEST(ConfigValidatorTest, ValidateFolderReportsCleanWhenConfigsValid)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    // A well-formed screen file should validate without diagnostics.
    writeFile(dir.path(), "screens/main.txt",
              "SCREEN AUTO AUTO 1.0\n"
              "VERTICAL\n"
              "  LABEL \"x\"\n"
              "END\n");

    const auto report = ConfigValidator::validateFolder(dir.path());
    EXPECT_FALSE(hasRule(report, "folder.empty"));
    EXPECT_TRUE(hasRule(report, "folder.clean"));
    EXPECT_EQ(report.errorCount(), 0);
}
