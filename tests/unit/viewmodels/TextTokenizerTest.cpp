#include "viewmodels/validation/TextTokenizer.h"
#include <gtest/gtest.h>

using OpenC3::ViewModels::Validation::tokenizeConfigLine;

// tokenizeConfigLine has no dedicated tests despite being the shared
// tokenizer behind every COSMOS config parser/validator in the codebase
// (PluginManifestParser, ScreenLayoutParser, ConfigValidator,
// CommandValidator, InterfaceValidator, TargetConfigValidator,
// ProtocolRuleSupport, TelemetryValidator, ProtocolValidator,
// PluginConfigParser, ScreenParser, ScreenValidator) - a bug here would
// silently corrupt every one of them at once.

TEST(TextTokenizerTest, SplitsOnSingleSpaces)
{
    const auto toks = tokenizeConfigLine("TARGET INST inst_target");
    ASSERT_EQ(toks.size(), 3);
    EXPECT_EQ(toks[0], "TARGET");
    EXPECT_EQ(toks[1], "INST");
    EXPECT_EQ(toks[2], "inst_target");
}

TEST(TextTokenizerTest, CollapsesRunsOfSpacesAndTabs)
{
    const auto toks = tokenizeConfigLine("TARGET   INST\tinst_target");
    ASSERT_EQ(toks.size(), 3);
    EXPECT_EQ(toks[0], "TARGET");
    EXPECT_EQ(toks[1], "INST");
    EXPECT_EQ(toks[2], "inst_target");
}

TEST(TextTokenizerTest, IgnoresLeadingAndTrailingWhitespace)
{
    const auto toks = tokenizeConfigLine("  TARGET INST  ");
    ASSERT_EQ(toks.size(), 2);
    EXPECT_EQ(toks[0], "TARGET");
    EXPECT_EQ(toks[1], "INST");
}

TEST(TextTokenizerTest, KeepsQuotedStringWithSpacesAsOneToken)
{
    const auto toks = tokenizeConfigLine(R"(DESCRIPTION "a multi word value")");
    ASSERT_EQ(toks.size(), 2);
    EXPECT_EQ(toks[0], "DESCRIPTION");
    EXPECT_EQ(toks[1], "a multi word value");
}

TEST(TextTokenizerTest, QuotedEmptyStringProducesAnEmptyToken)
{
    const auto toks = tokenizeConfigLine(R"(APPEND_PARAMETER "")");
    ASSERT_EQ(toks.size(), 2);
    EXPECT_EQ(toks[0], "APPEND_PARAMETER");
    EXPECT_EQ(toks[1], "");
}

TEST(TextTokenizerTest, MultipleQuotedTokensOnOneLine)
{
    const auto toks = tokenizeConfigLine(R"(SETTING "first one" "second one")");
    ASSERT_EQ(toks.size(), 3);
    EXPECT_EQ(toks[0], "SETTING");
    EXPECT_EQ(toks[1], "first one");
    EXPECT_EQ(toks[2], "second one");
}

TEST(TextTokenizerTest, EmptyLineProducesNoTokens)
{
    EXPECT_TRUE(tokenizeConfigLine("").isEmpty());
    EXPECT_TRUE(tokenizeConfigLine("   ").isEmpty());
}

TEST(TextTokenizerTest, UnterminatedQuoteStillYieldsTheAccumulatedToken)
{
    // Malformed input (missing closing quote) must not crash or drop data -
    // the parser layers above report their own diagnostics for malformed
    // lines; the tokenizer's job is just to not lose information.
    const auto toks = tokenizeConfigLine(R"(DESCRIPTION "unterminated)");
    ASSERT_EQ(toks.size(), 2);
    EXPECT_EQ(toks[0], "DESCRIPTION");
    EXPECT_EQ(toks[1], "unterminated");
}
