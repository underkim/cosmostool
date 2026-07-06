#include "viewmodels/plugin/PluginTemplateEngine.h"
#include "viewmodels/plugin/PluginManifestParser.h"
#include "viewmodels/cmdtlm/CmdTlmParser.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels;

TEST(PluginTemplateEngineTest, BuildFilesProducesExpectedTree)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "My description", 0);

    // Top-level plugin scaffolding.
    EXPECT_TRUE(files.contains("plugin.txt"));
    EXPECT_TRUE(files.contains("cosmos-my-plugin.gemspec"));
    EXPECT_TRUE(files.contains("Gemfile"));

    // Target files use the upper-cased target name in the path.
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_cmds.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_tlm.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/screens/fsw.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/procedures/fsw_check.rb"));
}

TEST(PluginTemplateEngineTest, PluginTxtSubstitutesNamesAndDescription)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "My description", 0);

    const QString plugin = files.value("plugin.txt");
    EXPECT_TRUE(plugin.contains("My description"));
    EXPECT_TRUE(plugin.contains("TARGET FSW"));
    // Hyphen in the plugin name is normalised to underscore for the variable.
    EXPECT_TRUE(plugin.contains("VARIABLE my_plugin_target_name"));
}

TEST(PluginTemplateEngineTest, PluginTxtIncludesCommentedRouterMicroserviceToolWidgetExamples)
{
    // Commented (not active) - a fresh plugin must never silently register a
    // non-functional ROUTER/MICROSERVICE, but the Manifest tab's "New Block"
    // workflow benefits from having a ready-made, correctly-shaped example to
    // uncomment and adjust rather than starting from a blank line.
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "My description", 0);
    const QString plugin = files.value("plugin.txt");

    EXPECT_TRUE(plugin.contains("# ROUTER FSW_ROUTER generic_router.rb"));
    EXPECT_TRUE(plugin.contains("# MICROSERVICE FSW_MICRO FSW_HEALTH_CHECK"));
    EXPECT_TRUE(plugin.contains("# TOOL FSW_tool"));
    EXPECT_TRUE(plugin.contains("# WIDGET FSW_widget"));

    // Must stay inactive: PluginManifestParser skips comment lines, so none
    // of these should surface as a real block until uncommented by hand.
    const auto parsed = PluginManifestParser::parse(plugin);
    for (const auto& block : parsed.blocks) {
        EXPECT_NE(block.kind, PluginManifestBlock::Kind::Router);
        EXPECT_NE(block.kind, PluginManifestBlock::Kind::Microservice);
        EXPECT_NE(block.kind, PluginManifestBlock::Kind::Tool);
        EXPECT_NE(block.kind, PluginManifestBlock::Kind::Widget);
    }
}

TEST(PluginTemplateEngineTest, GemspecCarriesGemName)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "desc", 0);

    const QString gemspec = files.value("cosmos-my-plugin.gemspec");
    EXPECT_TRUE(gemspec.contains("s.name        = 'cosmos-my-plugin'"));
}

TEST(PluginTemplateEngineTest, CcsdsTemplateDiffersFromGeneric)
{
    const auto generic = PluginTemplateEngine::buildFiles("p", "sat", "d", 0);
    const auto ccsds   = PluginTemplateEngine::buildFiles("p", "sat", "d", 2);

    // Template type 2 (GSE) uses a TCP/IP server interface.
    EXPECT_TRUE(ccsds.value("plugin.txt").contains("tcpip_server_interface"));
    EXPECT_TRUE(generic.value("plugin.txt").contains("tcpip_client_interface"));
}

TEST(PluginTemplateEngineTest, ExplicitInterfaceTypeOverridesTemplateDerivedDefault)
{
    // templateType 0 would normally default to a TCP/IP client interface, but
    // an explicit ifaceType should take priority (this is what the wizard's
    // Interface picker relies on - see PluginWizard.cpp).
    const auto udp = PluginTemplateEngine::buildFiles(
        "p", "sat", "d", /*templateType=*/0, /*ifaceType=*/2,
        "192.168.1.50", "9000");

    const QString plugin = udp.value("plugin.txt");
    EXPECT_TRUE(plugin.contains("udp_interface.rb 192.168.1.50 9000 9000 nil 10 nil"));
}

TEST(PluginTemplateEngineTest, SerialInterfaceUsesHostFieldAsDevicePath)
{
    const auto serial = PluginTemplateEngine::buildFiles(
        "p", "sat", "d", 0, /*ifaceType=*/3, "/dev/ttyUSB0", "9600");

    const QString plugin = serial.value("plugin.txt");
    EXPECT_TRUE(plugin.contains("serial_interface.rb /dev/ttyUSB0 9600 NONE 1 10 nil"));
}

TEST(PluginTemplateEngineTest, PluginNamespaceIsSurfacedInGeneratedCmdTlmFiles)
{
    // pluginNamespace previously had no effect on any generated output - the
    // wizard/dialog's "Namespace" field was silently discarded. It's now
    // surfaced as a header comment in the target's cmd/tlm files.
    const auto withNs = PluginTemplateEngine::buildTargetFiles("fsw", 0, "MySat");
    EXPECT_TRUE(withNs.value("targets/FSW/cmd_tlm/fsw_cmds.txt").startsWith("# Namespace: MySat\n"));
    EXPECT_TRUE(withNs.value("targets/FSW/cmd_tlm/fsw_tlm.txt").startsWith("# Namespace: MySat\n"));

    const auto withoutNs = PluginTemplateEngine::buildTargetFiles("fsw", 0);
    EXPECT_FALSE(withoutNs.value("targets/FSW/cmd_tlm/fsw_cmds.txt").contains("Namespace"));
}

TEST(PluginTemplateEngineTest, BuildTargetFilesOnlyEmitsTargetTree)
{
    const auto files = PluginTemplateEngine::buildTargetFiles("fsw", 0);

    // No top-level plugin files when adding a target to an existing plugin.
    EXPECT_FALSE(files.contains("plugin.txt"));
    EXPECT_FALSE(files.contains("Gemfile"));
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_cmds.txt"));
}

TEST(PluginTemplateEngineTest, BuildScriptFileEmitsSingleProcedureUnderUppercaseTarget)
{
    const auto files = PluginTemplateEngine::buildScriptFile("fsw", "MyCheck");

    ASSERT_EQ(files.size(), 1);
    ASSERT_TRUE(files.contains("targets/FSW/procedures/mycheck.rb"));

    const QString content = files.value("targets/FSW/procedures/mycheck.rb");
    EXPECT_TRUE(content.contains("def mycheck"));
    EXPECT_TRUE(content.contains("cmd('FSW PING')"));
    EXPECT_TRUE(content.contains("wait_check('FSW STATUS STATUS == \"RUNNING\"', 5)"));
    // The method is invoked at the bottom so running the script actually does something.
    EXPECT_TRUE(content.trimmed().endsWith("mycheck"));
}

namespace {
QString diagnosticsText(const CmdTlmParseResult& result)
{
    QStringList lines;
    for (const auto& d : result.diagnostics)
        lines << QString("line %1: %2").arg(d.line).arg(d.message);
    return lines.join('\n');
}
} // namespace

// The wizard's whole promise is "create a plugin that just works" - a
// generated cmd/tlm file that this app's own parser can't cleanly read
// would mean every freshly-scaffolded plugin fails Check before the user
// has even changed a thing. Covers both the generic (templateType 0) and
// CCSDS (templateType 1) templates, the two starting points offered in the
// wizard's "CMD/TLM Template" step.
TEST(PluginTemplateEngineTest, GeneratedCmdTlmFilesParseWithoutErrors)
{
    for (const int templateType : {0, 1}) {
        const auto files = PluginTemplateEngine::buildTargetFiles("fsw", templateType);

        const auto cmdsResult = CmdTlmParser::parse(files.value("targets/FSW/cmd_tlm/fsw_cmds.txt"));
        EXPECT_EQ(cmdsResult.errorCount(), 0)
            << "templateType=" << templateType << " cmds.txt diagnostics:\n"
            << diagnosticsText(cmdsResult).toStdString();

        const auto tlmResult = CmdTlmParser::parse(files.value("targets/FSW/cmd_tlm/fsw_tlm.txt"));
        EXPECT_EQ(tlmResult.errorCount(), 0)
            << "templateType=" << templateType << " tlm.txt diagnostics:\n"
            << diagnosticsText(tlmResult).toStdString();
    }
}
