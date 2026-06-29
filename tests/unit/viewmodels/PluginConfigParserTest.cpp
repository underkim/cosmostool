#include "viewmodels/validation/PluginConfigParser.h"
#include "viewmodels/validation/PluginValidator.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

TEST(PluginConfigParserTest, ValidPluginHasNoErrors)
{
    const QString src =
        "VARIABLE port 8080\n"
        "TARGET INST INST\n"
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080 8081 10.0 nil BURST\n"
        "  MAP_TARGET INST\n"
        "  PROTOCOL READ_WRITE length_protocol.rb 0 16 0\n";

    const auto result = PluginConfigParser::parse(src);
    EXPECT_EQ(result.report.errorCount(), 0);
    EXPECT_EQ(result.declaredTargets.size(), 1);
    EXPECT_EQ(result.declaredInterfaces.size(), 1);
}

// PROTOCOL validation now lives in the shared protocol rule, surfaced through
// PluginValidator for whole-plugin.txt checks.
TEST(PluginConfigParserTest, ProtocolOutsideInterfaceProducesWarning)
{
    const QString src =
        "TARGET INST INST\n"
        "PROTOCOL READ_WRITE length_protocol.rb\n";

    const auto report = PluginValidator{}.validate(src);
    EXPECT_GE(report.warningCount(), 1);
}

TEST(PluginConfigParserTest, ProtocolBadDirectionProducesError)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080\n"
        "  PROTOCOL SIDEWAYS length_protocol.rb\n";

    const auto report = PluginValidator{}.validate(src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(PluginConfigParserTest, InterfaceMissingClassProducesError)
{
    const QString src = "INTERFACE INST_INT\n";

    const auto result = PluginConfigParser::parse(src);
    EXPECT_GE(result.report.errorCount(), 1);
}

TEST(PluginConfigParserTest, MapTargetToUndeclaredProducesWarning)
{
    const QString src =
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080\n"
        "  MAP_TARGET NOPE\n";

    const auto result = PluginConfigParser::parse(src);
    EXPECT_EQ(result.report.errorCount(), 0);
    EXPECT_GE(result.report.warningCount(), 1);
}
