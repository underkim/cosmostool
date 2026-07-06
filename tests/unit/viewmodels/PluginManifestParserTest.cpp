#include "viewmodels/plugin/PluginManifestParser.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels;

TEST(PluginManifestParserTest, ParsesTargetVariableInterfaceMapTarget)
{
    // Same shape as PluginTemplateEngine's generated scaffold (resolved, not
    // the raw ERB template text).
    const QString src =
        "VARIABLE inst_target_name INST\n"
        "\n"
        "TARGET INST INST\n"
        "INTERFACE INST_INT tcpip_client_interface.rb host 8080 8081\n"
        "  MAP_TARGET INST\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 3);

    EXPECT_EQ(result.blocks[0].kind, PluginManifestBlock::Kind::Variable);
    EXPECT_EQ(result.blocks[0].name, "INST_TARGET_NAME");
    EXPECT_EQ(result.blocks[0].lineNumber, 1);

    EXPECT_EQ(result.blocks[1].kind, PluginManifestBlock::Kind::Target);
    EXPECT_EQ(result.blocks[1].classFileOrFolder, "INST");
    EXPECT_EQ(result.blocks[1].name, "INST");
    EXPECT_EQ(result.blocks[1].lineNumber, 3);
    EXPECT_TRUE(result.blocks[1].modifiers.isEmpty());

    const auto& iface = result.blocks[2];
    EXPECT_EQ(iface.kind, PluginManifestBlock::Kind::Interface);
    EXPECT_EQ(iface.name, "INST_INT");
    EXPECT_EQ(iface.classFileOrFolder, "tcpip_client_interface.rb");
    EXPECT_EQ(iface.extraArgs, QStringList({"host", "8080", "8081"}));
    EXPECT_EQ(iface.lineNumber, 4);
    ASSERT_EQ(iface.modifiers.size(), 1);
    EXPECT_EQ(iface.modifiers[0].keyword, "MAP_TARGET");
    EXPECT_EQ(iface.modifiers[0].lineNumber, 5);
}

TEST(PluginManifestParserTest, ParsesRouterWithProtocolAndOptionModifiers)
{
    const QString src =
        "ROUTER ROUTER1 generic_router.rb\n"
        "  PROTOCOL READ_WRITE PreidentifiedProtocol\n"
        "  OPTION LISTEN_ADDRESS 0.0.0.0\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 1);
    const auto& block = result.blocks[0];
    EXPECT_EQ(block.kind, PluginManifestBlock::Kind::Router);
    EXPECT_EQ(block.name, "ROUTER1");
    EXPECT_EQ(block.classFileOrFolder, "generic_router.rb");

    ASSERT_EQ(block.modifiers.size(), 2);
    EXPECT_EQ(block.modifiers[0].keyword, "PROTOCOL");
    EXPECT_EQ(block.modifiers[0].args, QStringList({"READ_WRITE", "PreidentifiedProtocol"}));
    EXPECT_EQ(block.modifiers[1].keyword, "OPTION");
    EXPECT_EQ(block.modifiers[1].args, QStringList({"LISTEN_ADDRESS", "0.0.0.0"}));
}

TEST(PluginManifestParserTest, ParsesMicroserviceWithEnvCmdTopicModifiers)
{
    const QString src =
        "MICROSERVICE MY_FOLDER MY_MICROSERVICE\n"
        "  ENV MY_ENV_VAR value1\n"
        "  CMD ruby my_microservice.rb\n"
        "  TOPIC MY_TOPIC\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 1);
    const auto& block = result.blocks[0];
    EXPECT_EQ(block.kind, PluginManifestBlock::Kind::Microservice);
    EXPECT_EQ(block.classFileOrFolder, "MY_FOLDER");
    EXPECT_EQ(block.name, "MY_MICROSERVICE");

    ASSERT_EQ(block.modifiers.size(), 3);
    EXPECT_EQ(block.modifiers[0].keyword, "ENV");
    EXPECT_EQ(block.modifiers[1].keyword, "CMD");
    EXPECT_EQ(block.modifiers[2].keyword, "TOPIC");
}

TEST(PluginManifestParserTest, ParsesToolAndWidgetWithModifiers)
{
    const QString src =
        "TOOL my_tool\n"
        "  URL /tools/my_tool\n"
        "  ICON mdi-rocket\n"
        "  CATEGORY \"Custom Tools\"\n"
        "WIDGET my_widget\n"
        "  URL /widgets/my_widget\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 2);

    const auto& tool = result.blocks[0];
    EXPECT_EQ(tool.kind, PluginManifestBlock::Kind::Tool);
    EXPECT_TRUE(tool.name.isEmpty());
    ASSERT_EQ(tool.modifiers.size(), 3);
    EXPECT_EQ(tool.modifiers[0].keyword, "URL");
    EXPECT_EQ(tool.modifiers[1].keyword, "ICON");
    EXPECT_EQ(tool.modifiers[2].keyword, "CATEGORY");
    EXPECT_EQ(tool.modifiers[2].args, QStringList({"Custom Tools"})); // quoted, space preserved

    const auto& widget = result.blocks[1];
    EXPECT_EQ(widget.kind, PluginManifestBlock::Kind::Widget);
    ASSERT_EQ(widget.modifiers.size(), 1);
    EXPECT_EQ(widget.modifiers[0].keyword, "URL");
}

TEST(PluginManifestParserTest, LineNumbersMatchLiteralSourcePosition)
{
    // The UI's surgical line-based edits depend entirely on lineNumber being
    // the true 1-based line of each block/modifier in the source text - a
    // regression here would silently corrupt unrelated lines, the same class
    // of bug previously fixed for CMD/TLM's orphaned sub-directive lines.
    const QString src =
        "# a leading comment\n"
        "\n"
        "VARIABLE port 8080\n"
        "\n"
        "INTERFACE INST_INT tcpip_client_interface.rb\n"
        "  PROTOCOL READ_WRITE Foo\n"
        "  OPTION BAR baz\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 2);
    EXPECT_EQ(result.blocks[0].lineNumber, 3);   // VARIABLE
    EXPECT_EQ(result.blocks[1].lineNumber, 5);   // INTERFACE
    ASSERT_EQ(result.blocks[1].modifiers.size(), 2);
    EXPECT_EQ(result.blocks[1].modifiers[0].lineNumber, 6);  // PROTOCOL
    EXPECT_EQ(result.blocks[1].modifiers[1].lineNumber, 7);  // OPTION
}

TEST(PluginManifestParserTest, UnknownTopLevelKeywordIsSkippedWithoutCrash)
{
    // NEEDS_DEPENDENCIES / SCRIPT_ENGINE are recognised top-level keywords
    // but not editable Manifest-tab block kinds - they should be silently
    // skipped (not surfaced as blocks, not crashing, no diagnostic noise).
    const QString src =
        "NEEDS_DEPENDENCIES\n"
        "SCRIPT_ENGINE rb RubyScriptEngine\n"
        "VARIABLE port 8080\n";

    const auto result = PluginManifestParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 1);
    EXPECT_EQ(result.blocks[0].kind, PluginManifestBlock::Kind::Variable);
}

TEST(PluginManifestParserTest, ModifierOutsideAnyBlockProducesDiagnosticNotCrash)
{
    const QString src = "PROTOCOL READ_WRITE Foo\n";

    const auto result = PluginManifestParser::parse(src);

    EXPECT_TRUE(result.blocks.isEmpty());
    EXPECT_GE(result.diagnostics.size(), 1);
}

TEST(PluginManifestParserTest, IsKnownTopLevelKeywordDelegatesToPluginKeywords)
{
    EXPECT_TRUE(PluginManifestParser::isKnownTopLevelKeyword("target"));
    EXPECT_TRUE(PluginManifestParser::isKnownTopLevelKeyword("INTERFACE"));
    EXPECT_FALSE(PluginManifestParser::isKnownTopLevelKeyword("NOT_A_KEYWORD"));
}
