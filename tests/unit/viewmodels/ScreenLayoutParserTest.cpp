#include "viewmodels/screen/ScreenLayoutParser.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels;

TEST(ScreenLayoutParserTest, ParsesHeaderTitleVerticalBoxLabelValueTree)
{
    // Same shape as PluginTemplateEngine's generated screen file
    // (buildScreenAndProcedures(), targets/<TGT>/screens/<tgt>.txt).
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "\n"
        "TITLE \"MYSAT Status\"\n"
        "\n"
        "VERTICALBOX\n"
        "  LABELVALUE MYSAT STATUS STATUS\n"
        "  LABELVALUE MYSAT STATUS COUNTER\n"
        "END\n";

    const auto result = ScreenLayoutParser::parse(src);

    EXPECT_TRUE(result.hasHeader);
    EXPECT_EQ(result.screenWidth, "AUTO");
    EXPECT_EQ(result.screenHeight, "AUTO");

    ASSERT_EQ(result.roots.size(), 2);
    EXPECT_EQ(result.roots[0].keyword, "TITLE");
    EXPECT_EQ(result.roots[0].args, QStringList({"MYSAT Status"}));
    EXPECT_FALSE(result.roots[0].isContainer);

    const auto& box = result.roots[1];
    EXPECT_EQ(box.keyword, "VERTICALBOX");
    EXPECT_TRUE(box.isContainer);
    ASSERT_EQ(box.children.size(), 2);
    EXPECT_EQ(box.children[0].keyword, "LABELVALUE");
    EXPECT_EQ(box.children[0].args, QStringList({"MYSAT", "STATUS", "STATUS"}));
    EXPECT_EQ(box.children[1].args, QStringList({"MYSAT", "STATUS", "COUNTER"}));
}

TEST(ScreenLayoutParserTest, ParsesNestedContainers)
{
    const QString src =
        "SCREEN AUTO AUTO 1.0\n"
        "VERTICALBOX\n"
        "  HORIZONTALBOX\n"
        "    LABEL \"Left\"\n"
        "    LABEL \"Right\"\n"
        "  END\n"
        "  BUTTON \"Click\" \"screen_close\"\n"
        "END\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    const auto& outer = result.roots[0];
    EXPECT_EQ(outer.keyword, "VERTICALBOX");
    ASSERT_EQ(outer.children.size(), 2);

    const auto& inner = outer.children[0];
    EXPECT_EQ(inner.keyword, "HORIZONTALBOX");
    EXPECT_TRUE(inner.isContainer);
    ASSERT_EQ(inner.children.size(), 2);
    EXPECT_EQ(inner.children[0].args, QStringList({"Left"}));
    EXPECT_EQ(inner.children[1].args, QStringList({"Right"}));

    EXPECT_EQ(outer.children[1].keyword, "BUTTON");
    EXPECT_EQ(outer.children[1].args, QStringList({"Click", "screen_close"}));
}

TEST(ScreenLayoutParserTest, UnwrapsNamedWidget)
{
    const QString src = "NAMED_WIDGET temp VALUE INST HEALTH_STATUS TEMP1\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    const auto& node = result.roots[0];
    EXPECT_EQ(node.keyword, "VALUE");
    EXPECT_EQ(node.namedWidgetName, "temp");
    EXPECT_EQ(node.args, QStringList({"INST", "HEALTH_STATUS", "TEMP1"}));
    EXPECT_FALSE(node.isContainer);
}

TEST(ScreenLayoutParserTest, UnclosedContainerStillAttachesChildrenWithoutCrashing)
{
    // No END - a best-effort tree, not a validator (ScreenParser already
    // reports "never closed" for this case).
    const QString src =
        "VERTICALBOX\n"
        "  LABEL \"Orphaned\"\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    EXPECT_TRUE(result.roots[0].isContainer);
    ASSERT_EQ(result.roots[0].children.size(), 1);
    EXPECT_EQ(result.roots[0].children[0].keyword, "LABEL");
}

TEST(ScreenLayoutParserTest, StrayEndIsIgnoredWithoutCrashing)
{
    const QString src =
        "END\n"
        "LABEL \"After stray END\"\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    EXPECT_EQ(result.roots[0].keyword, "LABEL");
}

TEST(ScreenLayoutParserTest, SettingLinesDoNotBecomeTheirOwnNode)
{
    const QString src =
        "LABEL \"Styled\"\n"
        "  SETTING BACKCOLOR blue\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    EXPECT_EQ(result.roots[0].keyword, "LABEL");
}

TEST(ScreenLayoutParserTest, UnknownWidgetStillBecomesALeafNode)
{
    // The preview renderer relies on unrecognised keywords still producing a
    // node (rendered as a placeholder box) rather than being dropped.
    const QString src = "TOTALLY_MADE_UP_WIDGET foo bar\n";

    const auto result = ScreenLayoutParser::parse(src);

    ASSERT_EQ(result.roots.size(), 1);
    EXPECT_EQ(result.roots[0].keyword, "TOTALLY_MADE_UP_WIDGET");
    EXPECT_EQ(result.roots[0].args, QStringList({"foo", "bar"}));
}
