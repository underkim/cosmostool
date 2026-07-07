#include "viewmodels/cmdtlm/CmdTlmParser.h"

#include <gtest/gtest.h>
#include <QString>

#include <algorithm>

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

TEST(CmdTlmParserTest, ParsesExplicitOffsetParameterAndItem)
{
    const QString src =
        "COMMAND TGT SET BIG_ENDIAN \"Set\"\n"
        "  PARAMETER MODE 0 8 UINT 0 255 0 \"Mode\"\n"
        "TELEMETRY TGT HK BIG_ENDIAN \"Housekeeping\"\n"
        "  ITEM TEMP 16 16 INT \"Temperature\"\n";

    const auto result = CmdTlmParser::parse(src);

    EXPECT_EQ(result.errorCount(), 0);
    ASSERT_EQ(result.blocks.size(), 2);
    ASSERT_EQ(result.blocks[0].items.size(), 1);
    EXPECT_EQ(result.blocks[0].items[0].name, "MODE");
    EXPECT_EQ(result.blocks[0].items[0].bitSize, 8);
    EXPECT_EQ(result.blocks[0].items[0].dataType, "UINT");
    EXPECT_EQ(result.blocks[0].items[0].minVal, "0");
    EXPECT_EQ(result.blocks[0].items[0].maxVal, "255");
    EXPECT_EQ(result.blocks[0].items[0].defaultVal, "0");

    ASSERT_EQ(result.blocks[1].items.size(), 1);
    EXPECT_EQ(result.blocks[1].items[0].name, "TEMP");
    EXPECT_EQ(result.blocks[1].items[0].bitSize, 16);
    EXPECT_EQ(result.blocks[1].items[0].dataType, "INT");
    EXPECT_EQ(result.blocks[1].items[0].description, "Temperature");
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

// An unknown modifier inside a TELEMETRY block belongs to that block only -
// it must not be tagged Scope::Any (the default), or it would leak into the
// CMD-only validator's report too (CommandValidator/TelemetryValidator filter
// on scope, but Any-scoped diagnostics always pass through both).
TEST(CmdTlmParserTest, UnknownKeywordInsideBlockIsScopedToThatBlock)
{
    const QString src =
        "TELEMETRY T H BIG_ENDIAN \"d\"\n"
        "  APPEND_ITEM FIELD1 16 UINT \"A field\"\n"
        "  BOGUS_MODIFIER foo\n";

    const auto result = CmdTlmParser::parse(src);

    const auto it = std::find_if(result.diagnostics.cbegin(), result.diagnostics.cend(),
        [](const CmdTlmDiagnostic& d) { return d.message.contains("BOGUS_MODIFIER"); });
    ASSERT_NE(it, result.diagnostics.cend());
    EXPECT_EQ(it->scope, CmdTlmDiagnostic::Scope::Telemetry);
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

TEST(CmdTlmParserTest, RecognisesSelectAndModifierKeywords)
{
    // SELECT_TELEMETRY then SELECT_ITEM to override an item, plus PROCESSOR
    // and other valid modifiers. None of these should be flagged as unknown.
    const QString src =
        "SELECT_TELEMETRY T H\n"
        "  SELECT_ITEM TEMP1\n"
        "    LIMITS DEFAULT 1 ENABLED -80 -70 60 80\n"
        "  PROCESSOR TEMP1STAT statistics_processor.rb TEMP1 100\n"
        "  DELETE_ITEM JUNK\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.warningCount(), 0);
    EXPECT_EQ(result.errorCount(), 0);
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

TEST(CmdTlmParserTest, NonIntegerBitOffsetProducesError)
{
    const QString src =
        "COMMAND TGT C BIG_ENDIAN \"d\"\n"
        "  PARAMETER P notanumber 16 UINT 0 100 5 \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_GE(result.errorCount(), 1);
}

TEST(CmdTlmParserTest, NonIntegerArrayBitSizeProducesError)
{
    const QString src =
        "TELEMETRY T H BIG_ENDIAN \"d\"\n"
        "  APPEND_ARRAY_ITEM ARR 16 UINT notanumber \"x\"\n";

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
    ASSERT_EQ(result.blocks.size(), 1);
    ASSERT_EQ(result.blocks[0].items.size(), 1);
    EXPECT_TRUE(result.blocks[0].items[0].isArray);
    EXPECT_EQ(result.blocks[0].items[0].arrayBitSize, 160);
}

TEST(CmdTlmParserTest, ArrayParameterKeepsRangeAndDefault)
{
    const QString src =
        "COMMAND T C BIG_ENDIAN \"d\"\n"
        "  APPEND_ARRAY_PARAMETER ARR 16 UINT 160 0 255 1 \"x\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
    ASSERT_EQ(result.blocks.size(), 1);
    ASSERT_EQ(result.blocks[0].items.size(), 1);
    EXPECT_EQ(result.blocks[0].items[0].arrayBitSize, 160);
    EXPECT_EQ(result.blocks[0].items[0].minVal, "0");
    EXPECT_EQ(result.blocks[0].items[0].maxVal, "255");
    EXPECT_EQ(result.blocks[0].items[0].defaultVal, "1");
    EXPECT_EQ(result.blocks[0].items[0].description, "x");
}

// APPEND_ID_ITEM's id_value token (right after the type, before the
// description) was previously just skipped (++idx) and never captured
// anywhere - PluginView's structure table reads item.defaultVal to display
// and to regenerate this row's line, so a discarded id_value meant editing
// any cell of an existing ID telemetry item silently dropped its numeric ID
// from the file on save.
TEST(CmdTlmParserTest, IdItemCapturesIdValueAndDescription)
{
    const QString src =
        "TELEMETRY T HK BIG_ENDIAN \"d\"\n"
        "  APPEND_ID_ITEM PKTID 8 UINT 5 \"Packet identifier\"\n";

    const auto result = CmdTlmParser::parse(src);
    EXPECT_EQ(result.errorCount(), 0);
    ASSERT_EQ(result.blocks.size(), 1);
    ASSERT_EQ(result.blocks[0].items.size(), 1);
    const auto& item = result.blocks[0].items[0];
    EXPECT_TRUE(item.isId);
    EXPECT_EQ(item.name, "PKTID");
    EXPECT_EQ(item.defaultVal, "5");
    EXPECT_EQ(item.description, "Packet identifier");
}

// Exposed for PluginView's field-delete flow, which needs to remove a
// field's trailing sub-directive lines (STATE, UNITS, ...) along with its
// own line - otherwise they'd be orphaned under whatever field ends up
// above once the parent is deleted, silently altering that unrelated field.
TEST(CmdTlmParserTest, IsSubDirectiveKeywordRecognisesKnownDirectivesOnly)
{
    EXPECT_TRUE(CmdTlmParser::isSubDirectiveKeyword("STATE"));
    EXPECT_TRUE(CmdTlmParser::isSubDirectiveKeyword("state"));
    EXPECT_TRUE(CmdTlmParser::isSubDirectiveKeyword("UNITS"));
    EXPECT_FALSE(CmdTlmParser::isSubDirectiveKeyword("APPEND_ITEM"));
    EXPECT_FALSE(CmdTlmParser::isSubDirectiveKeyword("TELEMETRY"));
    EXPECT_FALSE(CmdTlmParser::isSubDirectiveKeyword("NOT_A_KEYWORD"));
}
