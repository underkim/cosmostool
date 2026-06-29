#include "viewmodels/validation/ConfigValidator.h"
#include "viewmodels/validation/PluginConfigParser.h"
#include "viewmodels/validation/TargetConfigParser.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using namespace OpenC3::ViewModels::Validation;

namespace {

bool hasRule(const ValidationReport& report, const QString& rule)
{
    for (const auto& diagnostic : report.diagnostics) {
        if (diagnostic.rule == rule) return true;
    }
    return false;
}

void writeTextFile(const QString& path, const QString& content)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_EQ(file.write(content.toUtf8()), content.toUtf8().size());
}

} // namespace

TEST(PluginConfigParserTest, AcceptsTemplateVariableDeclaredBeforeUse)
{
    const QString src =
        "VARIABLE target_name INST\n"
        "TARGET INST <%= target_name %>\n"
        "INTERFACE INST_INT tcpip_client_interface.rb 127.0.0.1 8080 8080 10 nil BURST\n"
        "  MAP_TARGET <%= target_name %>\n";

    const ValidationReport report = PluginConfigParser::validate(src);

    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_FALSE(hasRule(report, QStringLiteral("plugin.variable.undefined")));
}

TEST(PluginConfigParserTest, ReportsUndefinedTemplateVariable)
{
    const QString src = "TARGET INST <%= missing_target %>\n";

    const ValidationReport report = PluginConfigParser::validate(src);

    EXPECT_GE(report.errorCount(), 1);
    EXPECT_TRUE(hasRule(report, QStringLiteral("plugin.variable.undefined")));
}

TEST(PluginConfigParserTest, ReportsKeywordArity)
{
    const QString src = "ENV ONLY_KEY\n";

    const ValidationReport report = PluginConfigParser::validate(src);

    EXPECT_GE(report.errorCount(), 1);
    EXPECT_TRUE(hasRule(report, QStringLiteral("plugin.keyword.arity")));
}

TEST(TargetConfigParserTest, ReportsDuplicateCommandAndTelemetryFiles)
{
    const QString src =
        "COMMANDS cmd_tlm/inst_cmds.txt\n"
        "COMMANDS cmd_tlm/inst_cmds.txt\n"
        "TELEMETRY cmd_tlm/inst_tlm.txt\n"
        "TELEMETRY cmd_tlm/inst_tlm.txt\n";

    const ValidationReport report = TargetConfigParser::validate(src);

    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_TRUE(hasRule(report, QStringLiteral("target.commands.duplicate")));
    EXPECT_TRUE(hasRule(report, QStringLiteral("target.telemetry.duplicate")));
}

TEST(TargetConfigParserTest, ReportsInvalidStalenessValue)
{
    const QString src =
        "COMMANDS cmd_tlm/inst_cmds.txt\n"
        "TELEMETRY cmd_tlm/inst_tlm.txt\n"
        "STALENESS_SECONDS soon\n";

    const ValidationReport report = TargetConfigParser::validate(src);

    EXPECT_GE(report.errorCount(), 1);
    EXPECT_TRUE(hasRule(report, QStringLiteral("target.staleness.numeric")));
}

TEST(ConfigValidatorTest, DispatchesPluginAndTargetFiles)
{
    EXPECT_EQ(ConfigValidator::classify(QStringLiteral("plugin.txt")), FileKind::PluginConfig);
    EXPECT_EQ(ConfigValidator::classify(QStringLiteral("targets/INST/target.txt")), FileKind::TargetConfig);
    EXPECT_EQ(ConfigValidator::classify(QStringLiteral("targets/INST/screens/status.txt")), FileKind::Unknown);
}

TEST(ConfigValidatorTest, ValidatesFolderAndReportsMissingPlugin)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir().mkpath(dir.filePath(QStringLiteral("targets/INST"))));
    writeTextFile(dir.filePath(QStringLiteral("targets/INST/target.txt")),
                  QStringLiteral("COMMANDS cmd_tlm/inst_cmds.txt\n"
                                 "TELEMETRY cmd_tlm/inst_tlm.txt\n"));

    const ValidationReport report = ConfigValidator::validateFolder(dir.path());

    EXPECT_GE(report.warningCount(), 1);
    EXPECT_TRUE(hasRule(report, QStringLiteral("config.folder.plugin_missing")));
}
