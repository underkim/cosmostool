#include "viewmodels/cmdtlm/CmdTlmParser.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels;

TEST(CmdTlmParserTest, ParsesCommandBlockWithItems)
{
    const QString src =
        "COMMAND TGT NOOP BIG_ENDIAN \"No operation\"\n"
        "  APPEND_PARAMETER FIELD1 16 UINT16 0 100 5 \"A field\"\n";

    const auto result = CmdTlmParser::parse(src);

    ASSERT_EQ(result.blocks.size(), 1);
    EXPECT_EQ(result.errorCount(), 0);

    const auto& block = result.blocks[0];
    EXPECT_EQ(block.kind, CmdTlmBlock::Kind::Command);
    EXPECT_EQ(block.targetName, "TGT");
    EXPECT_EQ(block.name, "NOOP");
    EXPECT_EQ(block.endianness, "BIG_ENDIAN");
    EXPECT_EQ(block.description, "No operation");  // quoted, space preserved

    ASSERT_EQ(block.items.size(), 1);
    EXPECT_EQ(block.items[0].name, "FIELD1");
    EXPECT_EQ(block.items[0].bitSize, 16);
    EXPECT_EQ(block.items[0].dataType, "UINT16");
}

TEST(CmdTlmParserTest, UnknownDataTypeProducesError)
{
    const QString src =
        "TELEMETRY TGT HK BIG_ENDIAN \"Housekeeping\"\n"
        "  APPEND_ITEM X 8 FOOBAR \"bad type\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, AcceptsCommonOpenC3TypeAliases)
{
    const QString src =
        "COMMAND TGT SET BIG_ENDIAN \"Set values\"\n"
        "  APPEND_PARAMETER COUNT 16 UINT 0 65535 0 \"Count\"\n"
        "  APPEND_PARAMETER OFFSET 16 INT -10 10 0 \"Offset\"\n"
        "  APPEND_PARAMETER GAIN 32 FLOAT 0 10 1.0 \"Gain\"\n"
        "TELEMETRY TGT HK BIG_ENDIAN \"Housekeeping\"\n"
        "  APPEND_ITEM TEMP 16 INT \"Temperature\"\n"
        "  APPEND_ITEM VOLTAGE 32 FLOAT \"Voltage\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
}

TEST(CmdTlmParserTest, ItemOutsideBlockProducesError)
{
    const QString src = "APPEND_ITEM X 8 UINT8 \"orphan\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, DuplicateItemNameProducesError)
{
    const QString src =
        "TELEMETRY TGT HK BIG_ENDIAN \"hk\"\n"
        "  APPEND_ITEM DUP 8 UINT8 \"first\"\n"
        "  APPEND_ITEM DUP 8 UINT8 \"second\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, UnrecognisedEndiannessProducesWarning)
{
    const QString src = "COMMAND TGT NOOP MIDDLE_ENDIAN \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
    EXPECT_GE(result.warningCount(), 1);
}

TEST(CmdTlmParserTest, UnknownKeywordProducesWarning)
{
    const QString src = "BOGUS_DIRECTIVE foo bar\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.warningCount(), 1);
}

TEST(CmdTlmParserTest, CommentsAndBlankLinesIgnored)
{
    const QString src =
        "# a comment\n"
        "\n"
        "   \n"
        "COMMAND TGT NOOP BIG_ENDIAN \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    ASSERT_EQ(result.blocks.size(), 1);
    EXPECT_EQ(result.errorCount(), 0);
}

TEST(CmdTlmParserTest, MissingTargetOrNameProducesError)
{
    const QString src = "COMMAND\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, NonAppendParameterUsesBitOffsetColumn)
{
    const QString src =
        "COMMAND TGT C BIG_ENDIAN \"d\"\n"
        "  PARAMETER P 0 16 UINT 0 100 5 \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
    ASSERT_EQ(result.blocks.size(), 1);
    ASSERT_EQ(result.blocks[0].items.size(), 1);
    EXPECT_EQ(result.blocks[0].items[0].name, "P");
    EXPECT_EQ(result.blocks[0].items[0].bitSize, 16);
    EXPECT_EQ(result.blocks[0].items[0].dataType, "UINT");
}

TEST(CmdTlmParserTest, FloatWithBadBitSizeProducesError)
{
    const QString src =
        "TELEMETRY T H BIG_ENDIAN \"d\"\n"
        "  APPEND_ITEM F 16 FLOAT \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, SelectTelemetryWithoutItemsIsOk)
{
    const QString src = "SELECT_TELEMETRY T H\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
    EXPECT_EQ(result.warningCount(), 0);
}

TEST(CmdTlmParserTest, LimitsWrongArgumentCountProducesError)
{
    const QString src =
        "TELEMETRY T H BIG_ENDIAN \"d\"\n"
        "  APPEND_ITEM X 8 UINT \"x\"\n"
        "  LIMITS DEFAULT 1 ENABLED 1 2\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, ArrayItemIsAccepted)
{
    const QString src =
        "TELEMETRY T H BIG_ENDIAN \"d\"\n"
        "  APPEND_ARRAY_ITEM ARR 16 UINT 160 \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
}
