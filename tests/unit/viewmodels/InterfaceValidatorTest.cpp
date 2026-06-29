#include "viewmodels/validation/InterfaceValidator.h"

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

TEST(InterfaceValidatorTest, ValidInterfaceHasNoErrors)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080 8080 10.0 nil BURST\n"
        "  MAP_TARGET INST\n"
        "  PROTOCOL READ_WRITE burst_protocol.rb\n";

    const auto report = InterfaceValidator{}.validate(src);
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(InterfaceValidatorTest, MissingClassIsError)
{
    const QString src = "INTERFACE INST_INT\n";

    const auto report = InterfaceValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
    EXPECT_GE(countRule(report, "interface.args"), 1);
}

TEST(InterfaceValidatorTest, UnknownModifierWarns)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080\n"
        "  NONSENSE_MODIFIER 1\n";

    const auto report = InterfaceValidator{}.validate(src);
    EXPECT_GE(countRule(report, "interface.modifier.unknown"), 1);
}

TEST(InterfaceValidatorTest, DelegatesProtocolChecks)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080\n"
        "  PROTOCOL SIDEWAYS burst_protocol.rb\n";

    const auto report = InterfaceValidator{}.validate(src);
    EXPECT_GE(countRule(report, "protocol.direction"), 1);
}

TEST(InterfaceValidatorTest, NoInterfaceReportsInfo)
{
    const QString src = "TARGET INST INST\n";

    const auto report = InterfaceValidator{}.validate(src);
    EXPECT_GE(countRule(report, "interface.none"), 1);
}
