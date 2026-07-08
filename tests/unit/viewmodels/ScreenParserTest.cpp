#include "viewmodels/validation/ScreenParser.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels::Validation;

TEST(ScreenParserTest, ValidScreenHasNoErrors)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "VERTICAL\n"
        "  LABELVALUE INST HEALTH_STATUS TEMP1\n"
        "  LABEL \"Status\"\n"
        "END\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(ScreenParserTest, MissingHeaderProducesError)
{
    const QString src =
        "VERTICAL\n"
        "  LABEL \"x\"\n"
        "END\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(ScreenParserTest, UnmatchedEndProducesError)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "LABEL \"x\"\n"
        "END\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(ScreenParserTest, UnclosedLayoutProducesError)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "VERTICAL\n"
        "  LABEL \"x\"\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_GE(report.errorCount(), 1);
}

TEST(ScreenParserTest, UnknownWidgetProducesWarning)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "WIDGETZZZ foo\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_EQ(report.errorCount(), 0);
    EXPECT_GE(report.warningCount(), 1);
}

TEST(ScreenParserTest, NamedWidgetIsAccepted)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "NAMED_WIDGET temp VALUE INST HEALTH_STATUS TEMP1\n";

    const auto report = ScreenParser::parse(src);
    EXPECT_EQ(report.errorCount(), 0);
}

TEST(ScreenParserTest, NamedWidgetWithUnknownTypeNamesTheWidgetTypeNotNamedWidget)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "NAMED_WIDGET temp BOGUSWIDGET foo\n";

    const auto report = ScreenParser::parse(src);
    ASSERT_GE(report.warningCount(), 1);
    const QString& message = report.diagnostics.last().message;
    EXPECT_TRUE(message.contains("BOGUSWIDGET"));
    EXPECT_FALSE(message.contains("NAMED_WIDGET"));
}
