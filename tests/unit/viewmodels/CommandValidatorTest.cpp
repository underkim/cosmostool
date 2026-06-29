#include "viewmodels/validation/CommandValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

namespace {

// A snippet with a clean COMMAND but a broken TELEMETRY (bad data type). The
// command validator must report nothing for it, because the only fault is in a
// TELEMETRY block.
const char* kMixed =
    "COMMAND INST COLLECT BIG_ENDIAN \"Collect command\"\n"
    "  APPEND_PARAMETER DURATION 32 FLOAT 0.0 10.0 1.0 \"Duration\"\n"
    "TELEMETRY INST HEALTH BIG_ENDIAN \"Health\"\n"
    "  APPEND_ITEM TEMP 16 BOGUSTYPE \"Temperature\"\n";

} // namespace

TEST(CommandValidatorTest, CleanCommandHasNoFindings)
{
    const QString src =
        "COMMAND INST COLLECT BIG_ENDIAN \"Collect\"\n"
        "  APPEND_PARAMETER DURATION 32 FLOAT 0.0 10.0 1.0 \"Duration\"\n";

    const auto report = CommandValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_EQ(report.warningCount(), 0);
}

TEST(CommandValidatorTest, IgnoresTelemetryOnlyFaults)
{
    const auto report = CommandValidator{}.validate(QString::fromUtf8(kMixed));
    // The only fault (BOGUSTYPE) is in a TELEMETRY block, so CMD sees nothing.
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(CommandValidatorTest, ReportsCommandFault)
{
    const QString src =
        "COMMAND INST COLLECT BIG_ENDIAN \"Collect\"\n"
        "  APPEND_PARAMETER DURATION 32 BOGUSTYPE 0 10 1 \"Duration\"\n";

    const auto report = CommandValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(CommandValidatorTest, BlockLessIssueStillSurfaces)
{
    // An item outside any block is a structural (scope-agnostic) error.
    const QString src = "APPEND_ITEM ORPHAN 16 UINT\n";

    const auto report = CommandValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
}
