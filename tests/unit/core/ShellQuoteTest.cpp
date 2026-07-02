#include "core/connection/ShellQuote.h"

#include <gtest/gtest.h>

using OpenC3::Core::Connection::shellQuote;
using OpenC3::Core::Connection::contentEndsHeredoc;
using OpenC3::Core::Connection::base64Encode;

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

// ── base64Encode ──────────────────────────────────────────────────────────────
// Reference values cross-checked against Python's base64.b64encode(). The
// encoder always appends a trailing newline after the (only, since inputs
// here are short) wrapped line — see the 76-column wrapping in the impl.

TEST(ShellQuoteTest, Base64EmptyInputIsEmpty)
{
    EXPECT_EQ(base64Encode(""), "");
}

TEST(ShellQuoteTest, Base64SingleByteIsPadded)
{
    EXPECT_EQ(base64Encode("f"), "Zg==\n");
}

TEST(ShellQuoteTest, Base64NoPaddingWhenLengthIsMultipleOfThree)
{
    EXPECT_EQ(base64Encode("foobar"), "Zm9vYmFy\n");
}

TEST(ShellQuoteTest, Base64HandlesEmbeddedNulAndHighBytes)
{
    // Binary content (a gem archive, an image, ...) must round-trip through
    // the ASCII-only alphabet without truncating at an embedded NUL.
    const std::string binary{ static_cast<char>(0x00), static_cast<char>(0x01),
                              static_cast<char>(0xFF) };
    EXPECT_EQ(base64Encode(binary), "AAH/\n");
}

TEST(ShellQuoteTest, Base64OutputIsPureAsciiAlphabetPlusWhitespace)
{
    // The whole point of base64-encoding a file before piping it through a
    // shell heredoc: the output must never contain characters that could be
    // mistaken for shell metacharacters or a heredoc delimiter line.
    std::string binary;
    for (int b = 0; b < 256; ++b)
        binary += static_cast<char>(b);
    const std::string encoded = base64Encode(binary);
    for (const char c : encoded) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '/' ||
                        c == '=' || c == '\n';
        EXPECT_TRUE(ok) << "unexpected character in base64 output: "
                        << static_cast<int>(static_cast<unsigned char>(c));
    }
}
