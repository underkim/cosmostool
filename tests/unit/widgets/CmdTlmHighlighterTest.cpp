#include "ui/widgets/CmdTlmHighlighter.h"

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QColor>

using OpenC3::UI::Widgets::CmdTlmHighlighter;

namespace {

// QSyntaxHighlighter defers its reformat-on-change work to the event loop
// (confirmed empirically: without pumping events, block.layout()->formats()
// stays empty even right after setPlainText()) - unlike the rest of this
// GTest suite, which is pure non-GUI viewmodels/services with no such
// requirement, so gtest_main's default main() never creates a
// QCoreApplication. Construct a process-lifetime instance here, and flush
// pending work after every edit via forceHighlight() below.
int fakeArgc = 1;
char fakeArgName[] = "opencosmos_tests";
char* fakeArgv[] = {fakeArgName, nullptr};

void forceHighlight(CmdTlmHighlighter& highlighter)
{
    static QCoreApplication app(fakeArgc, fakeArgv);
    (void)app;
    QCoreApplication::processEvents();
    highlighter.rehighlight();
    QCoreApplication::processEvents();
}

// Ruby COSMOS-API-call color and the shared string-literal color, matching
// CmdTlmHighlighter.cpp's own choices. kRubyApiCallColor is the exact rule
// whose raw-string literal previously had its lookahead's closing paren
// silently swallowed by the R"(...)" terminator, leaving highlightBlock()
// running an invalid QRegularExpression that matched nothing (Qt logs a
// warning rather than crashing, so this went unnoticed until the Korean
// screenshot smoke test happened to open a .rb file). Checking the
// resulting QTextCharFormat is the only way to catch that class of bug
// without a GUI harness.
const QColor kRubyApiCallColor("#4EC994");
const QColor kStringColor("#D69D85");

bool blockHasForeground(const QTextBlock& block, const QColor& color)
{
    for (const auto& range : block.layout()->formats())
        if (range.format.foreground().color() == color)
            return true;
    return false;
}

} // namespace

TEST(CmdTlmHighlighterTest, RubyModeHighlightsCosmosApiCalls)
{
    QTextDocument doc;
    CmdTlmHighlighter highlighter(&doc);
    highlighter.setRubyMode(true);

    doc.setPlainText("cmd('MYSAT PING')\nwait_check('MYSAT STATUS STATUS == 1', 5)\n");
    forceHighlight(highlighter);

    EXPECT_TRUE(blockHasForeground(doc.findBlockByNumber(0), kRubyApiCallColor));
    EXPECT_TRUE(blockHasForeground(doc.findBlockByNumber(1), kRubyApiCallColor));
}

TEST(CmdTlmHighlighterTest, RubyModeDoesNotFlagBareIdentifiersAsApiCalls)
{
    // The API-call rule's lookahead (?=\s*\() requires an opening paren -
    // "cmd" with no call syntax (e.g. a variable named "cmd_status") must
    // not be painted as if it were a real API call. Also confirm
    // highlighting genuinely ran (string color present) so a "no ruby
    // color anywhere" false-negative (e.g. from a broken regex again)
    // can't slip this test through vacuously.
    QTextDocument doc;
    CmdTlmHighlighter highlighter(&doc);
    highlighter.setRubyMode(true);

    doc.setPlainText("cmd_status = 'not a call'\n");
    forceHighlight(highlighter);

    EXPECT_FALSE(blockHasForeground(doc.findBlockByNumber(0), kRubyApiCallColor));
    EXPECT_TRUE(blockHasForeground(doc.findBlockByNumber(0), kStringColor));
}

TEST(CmdTlmHighlighterTest, NonRubyModeDoesNotApplyRubyRules)
{
    // rubyMode_ defaults to false - a COSMOS DSL file containing the word
    // "cmd(" (unlikely, but not this rule's concern) must not be painted
    // by the Ruby-only ruleset. Also confirm highlighting genuinely ran
    // (the DSL ruleset's own string-literal rule) for the same reason as
    // the test above.
    QTextDocument doc;
    CmdTlmHighlighter highlighter(&doc);

    doc.setPlainText("cmd(\"should not match in DSL mode\")\n");
    forceHighlight(highlighter);

    EXPECT_FALSE(blockHasForeground(doc.findBlockByNumber(0), kRubyApiCallColor));
    EXPECT_TRUE(blockHasForeground(doc.findBlockByNumber(0), kStringColor));
}
