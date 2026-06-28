#include "core/connection/ShellQuote.h"

#include <gtest/gtest.h>

using OpenC3::Core::Connection::shellQuote;

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
