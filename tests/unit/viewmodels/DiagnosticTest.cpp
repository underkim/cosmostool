#include "viewmodels/validation/Diagnostic.h"
#include <gtest/gtest.h>

using namespace OpenC3::ViewModels::Validation;

TEST(DiagnosticTest, FactoryMethodsSetTheRightSeverity)
{
    const auto e = Diagnostic::error(1, "msg", "rule.id", "fix it");
    EXPECT_EQ(e.severity, Diagnostic::Severity::Error);
    EXPECT_EQ(e.line, 1);
    EXPECT_EQ(e.message, "msg");
    EXPECT_EQ(e.rule, "rule.id");
    EXPECT_EQ(e.suggestion, "fix it");
    EXPECT_EQ(e.severityLabel(), "Error");

    EXPECT_EQ(Diagnostic::warning(2, "w").severity, Diagnostic::Severity::Warning);
    EXPECT_EQ(Diagnostic::warning(2, "w").severityLabel(), "Warning");
    EXPECT_EQ(Diagnostic::info(3, "i").severity, Diagnostic::Severity::Info);
    EXPECT_EQ(Diagnostic::info(3, "i").severityLabel(), "Info");
}

TEST(DiagnosticTest, CountsBySeverityAreIndependent)
{
    ValidationReport report;
    report.add(Diagnostic::error(1, "e1"));
    report.add(Diagnostic::error(2, "e2"));
    report.add(Diagnostic::warning(3, "w1"));
    report.add(Diagnostic::info(4, "i1"));

    EXPECT_EQ(report.errorCount(), 2);
    EXPECT_EQ(report.warningCount(), 1);
    EXPECT_EQ(report.infoCount(), 1);
    EXPECT_FALSE(report.ok());
}

TEST(DiagnosticTest, OkIsTrueOnlyWhenThereAreNoErrors)
{
    ValidationReport report;
    EXPECT_TRUE(report.ok());

    report.add(Diagnostic::warning(1, "w"));
    EXPECT_TRUE(report.ok()); // warnings alone don't fail a report

    report.add(Diagnostic::error(2, "e"));
    EXPECT_FALSE(report.ok());
}

TEST(DiagnosticTest, MergeAppendsOtherReportsDiagnostics)
{
    ValidationReport a, b;
    a.add(Diagnostic::error(1, "from a"));
    b.add(Diagnostic::error(2, "from b"));
    b.add(Diagnostic::warning(3, "also from b"));

    a.merge(b);
    EXPECT_EQ(a.diagnostics.size(), 3);
    EXPECT_EQ(a.errorCount(), 2);
    EXPECT_EQ(a.warningCount(), 1);
}

// setFilePath is used to stamp a path onto diagnostics from a validator that
// doesn't know its own source (in-memory content) - but ConfigValidator's
// folder-scan path merges several per-file reports together first and calls
// setFilePath once per file, so it must only fill in diagnostics that don't
// already have a path, or a later stamp would overwrite an earlier file's
// diagnostics with the wrong path.
TEST(DiagnosticTest, SetFilePathOnlyFillsDiagnosticsWithoutAnExistingPath)
{
    ValidationReport report;
    Diagnostic withPath = Diagnostic::error(1, "already tagged");
    withPath.filePath   = "existing.txt";
    report.add(withPath);
    report.add(Diagnostic::error(2, "untagged"));

    report.setFilePath("new.txt");

    EXPECT_EQ(report.diagnostics[0].filePath, "existing.txt");
    EXPECT_EQ(report.diagnostics[1].filePath, "new.txt");
}

TEST(DiagnosticTest, SummaryPluralizesCorrectlyAndOmitsZeroInfo)
{
    ValidationReport report;
    report.add(Diagnostic::error(1, "e"));
    report.add(Diagnostic::warning(2, "w"));
    EXPECT_EQ(report.summary(), "1 error, 1 warning");

    report.add(Diagnostic::error(3, "e2"));
    report.add(Diagnostic::warning(4, "w2"));
    EXPECT_EQ(report.summary(), "2 errors, 2 warnings");

    report.add(Diagnostic::info(5, "i"));
    EXPECT_EQ(report.summary(), "2 errors, 2 warnings, 1 info");
}
