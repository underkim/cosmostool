#include "app/StartupPolicy.h"

#include <gtest/gtest.h>

using namespace OpenC3;

TEST(StartupPolicyTest, PluginCreationNeverShowsDialogRegardlessOfProfiles)
{
    EXPECT_FALSE(App::shouldShowStartupConnectionDialog(UI::AppMode::PluginCreation, true));
    EXPECT_FALSE(App::shouldShowStartupConnectionDialog(UI::AppMode::PluginCreation, false));
}

TEST(StartupPolicyTest, ConnectOperateAlwaysShowsDialogRegardlessOfProfiles)
{
    EXPECT_TRUE(App::shouldShowStartupConnectionDialog(UI::AppMode::ConnectOperate, true));
    EXPECT_TRUE(App::shouldShowStartupConnectionDialog(UI::AppMode::ConnectOperate, false));
}
