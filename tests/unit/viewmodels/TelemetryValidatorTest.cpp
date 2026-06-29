#include "viewmodels/validation/TelemetryValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

namespace {

// Clean TELEMETRY, broken COMMAND (min > max). The telemetry validator must
// report nothing.
const char* kMixed =
    "COMMAND INST COLLECT BIG_ENDIAN \"Collect\"\n"
    "  APPEND_PARAMETER DURATION 32 UINT 100 1 50 \"Bad range\"\n"
    "TELEMETRY INST HEALTH BIG_ENDIAN \"Health\"\n"
    "  APPEND_ITEM TEMP 16 UINT \"Temperature\"\n";

} // namespace

TEST(TelemetryValidatorTest, CleanTelemetryHasNoFindings)
{
    const QString src =
        "TELEMETRY INST HEALTH BIG_ENDIAN \"Health\"\n"
        "  APPEND_ITEM TEMP 16 UINT \"Temperature\"\n";

    const auto report = TelemetryValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_EQ(report.warningCount(), 0);
}

TEST(TelemetryValidatorTest, IgnoresCommandOnlyFaults)
{
    const auto report = TelemetryValidator{}.validate(QString::fromUtf8(kMixed));
    // The min>max fault is in a COMMAND block, so TLM sees nothing.
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(TelemetryValidatorTest, ReportsTelemetryFault)
{
    const QString src =
        "TELEMETRY INST HEALTH BIG_ENDIAN \"Health\"\n"
        "  APPEND_ITEM TEMP 16 BOGUSTYPE \"Temperature\"\n";

    const auto report = TelemetryValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
}
