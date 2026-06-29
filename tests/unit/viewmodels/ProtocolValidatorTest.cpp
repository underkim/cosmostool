#include "viewmodels/validation/ProtocolValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

namespace {

int countRule(const ValidationReport& report, const char* rule)
{
    int n = 0;
    for (const auto& d : report.diagnostics)
        if (d.rule == QString::fromUtf8(rule))
            ++n;
    return n;
}

} // namespace

TEST(ProtocolValidatorTest, ValidBurstProtocolHasNoErrors)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080 8080 10.0 nil BURST\n"
        "  PROTOCOL READ_WRITE burst_protocol.rb 0 nil false\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(ProtocolValidatorTest, BadDirectionIsError)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb\n"
        "  PROTOCOL SIDEWAYS burst_protocol.rb\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
    EXPECT_GE(countRule(report, "protocol.direction"), 1);
}

TEST(ProtocolValidatorTest, MissingRequiredArgsWarns)
{
    // TerminatedProtocol requires write + read termination characters (2 args).
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb\n"
        "  PROTOCOL READ_WRITE terminated_protocol.rb\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_GE(countRule(report, "protocol.class.args"), 1);
}

TEST(ProtocolValidatorTest, ProtocolOutsideInterfaceWarns)
{
    const QString src = "PROTOCOL READ burst_protocol.rb\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_GE(countRule(report, "protocol.context"), 1);
}

TEST(ProtocolValidatorTest, CustomProtocolReportedAsInfo)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb\n"
        "  PROTOCOL READ my_custom_protocol.rb 1 2 3\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_GE(countRule(report, "protocol.class.custom"), 1);
}

TEST(ProtocolValidatorTest, ContextResetsAfterTarget)
{
    // A TARGET declaration closes the interface context; a following PROTOCOL
    // (unusual, but we should catch it) is out of context.
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb\n"
        "  PROTOCOL READ burst_protocol.rb\n"
        "TARGET INST INST\n"
        "PROTOCOL READ burst_protocol.rb\n";

    const auto report = ProtocolValidator{}.validate(src);
    EXPECT_GE(countRule(report, "protocol.context"), 1);
}
