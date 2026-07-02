#include "core/connection/ShellQuote.h"

#include <gtest/gtest.h>

using OpenC3::Core::Connection::shellQuote;
using OpenC3::Core::Connection::contentEndsHeredoc;

TEST(ShellQuoteTest, SimpleValueIsWrappedInSingleQuotes)
{
    EXPECT_EQ(shellQuote("hello"), "'hello'");
}

TEST(ShellQuoteTest, ValueWithSpaceIsQuoted)
{
    EXPECT_EQ(shellQuote("hello world"), "'hello world'");
}

TEST(ShellQuoteTest, EmbeddedSingleQuoteIsEscaped)
{
    EXPECT_EQ(shellQuote("a'b"), "'a'\\''b'");
}

TEST(ShellQuoteTest, ShellMetacharactersAreNeutralised)
{
    EXPECT_EQ(shellQuote("$(rm -rf /)"), "'$(rm -rf /)'");
}

TEST(ShellQuoteTest, EmptyStringBecomesEmptyQuotes)
{
    EXPECT_EQ(shellQuote(""), "''");
}

TEST(ShellQuoteTest, OnlySingleQuote)
{
    EXPECT_EQ(shellQuote("'"), "''\\'''");
}

TEST(ShellQuoteTest, BacktickAndSemicolonAndPipeAreQuoted)
{
    EXPECT_EQ(shellQuote("`whoami`; ls | cat"), "'`whoami`; ls | cat'");
}

TEST(ShellQuoteTest, PathWithSpacesIsQuoted)
{
    EXPECT_EQ(shellQuote("/cosmos/my plugins/x"),
              "'/cosmos/my plugins/x'");
}

// ── contentEndsHeredoc ────────────────────────────────────────────────────────

TEST(ShellQuoteTest, HeredocDetectsDelimiterAsWholePayload)
{
    EXPECT_TRUE(contentEndsHeredoc("SSHEOF", "SSHEOF"));
}

TEST(ShellQuoteTest, HeredocDetectsDelimiterOnFirstMiddleLastLine)
{
    EXPECT_TRUE(contentEndsHeredoc("SSHEOF\nrest", "SSHEOF"));      // first line
    EXPECT_TRUE(contentEndsHeredoc("a\nSSHEOF\nb", "SSHEOF"));     // middle line
    EXPECT_TRUE(contentEndsHeredoc("a\nSSHEOF", "SSHEOF"));        // last line
}

TEST(ShellQuoteTest, HeredocIgnoresDelimiterAsSubstring)
{
    // Only a whole line equal to the delimiter closes a heredoc.
    EXPECT_FALSE(contentEndsHeredoc("xSSHEOF\nSSHEOFy\n abc", "SSHEOF"));
    EXPECT_FALSE(contentEndsHeredoc("COMMAND TGT NOOP\n  PARAMETER X 8 UINT", "SSHEOF"));
    EXPECT_FALSE(contentEndsHeredoc("", "SSHEOF"));
}
