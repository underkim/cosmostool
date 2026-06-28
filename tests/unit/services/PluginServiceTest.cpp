#include "services/plugin/PluginService.h"
#include "mocks/MockCommandExecutor.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class PluginServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor mock_;
    PluginService       sut_{mock_};
};

TEST_F(PluginServiceTest, ListInstalledScansPluginFolders)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-my-plugin\t/cosmos/plugins/cosmos-my-plugin\t"
            "/cosmos/plugins/cosmos-my-plugin/plugin.txt\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin-0.1.0.gem|FSW,ADCS,\n")));

    const auto plugins = sut_.listInstalled("/cosmos");
    ASSERT_EQ(plugins.size(), 1u);
    EXPECT_EQ(plugins[0].name, "cosmos-my-plugin");
    EXPECT_EQ(plugins[0].rootPath, "/cosmos/plugins/cosmos-my-plugin");
    EXPECT_EQ(plugins[0].pluginTxtPath, "/cosmos/plugins/cosmos-my-plugin/plugin.txt");
    EXPECT_EQ(plugins[0].gemspecPath, "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec");
    EXPECT_EQ(plugins[0].targets.size(), 2u);
}

TEST_F(PluginServiceTest, ListInstalledAcceptsOpenC3ScriptPath)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto plugins = sut_.listInstalled("/cosmos/openc3.sh");
    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginServiceTest, ValidateQuotesLocalPath)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("p='/tmp/my plugin'")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto result = sut_.validate("/tmp/my plugin");
    EXPECT_TRUE(result.valid);
}

TEST_F(PluginServiceTest, ValidateReportsMissingPluginTxt)
{
    EXPECT_CALL(mock_, execute(_))
        .WillOnce(Return(ExecutorResult::ok("MISSING_PLUGIN_TXT\n")));

    const auto result = sut_.validate("/tmp/plugin");
    EXPECT_FALSE(result.valid);
    ASSERT_EQ(result.issues.size(), 1u);
    EXPECT_EQ(result.issues[0].code, "MISSING_PLUGIN_TXT");
}

TEST_F(PluginServiceTest, InstallQuotesBothArguments)
{
    EXPECT_CALL(mock_,
        execute("cp '/tmp/a.gem' '/cosmos/plugins/' 2>&1"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.install("/tmp/a.gem", "/cosmos"));
}

TEST_F(PluginServiceTest, RemoveQuotesConstructedPath)
{
    EXPECT_CALL(mock_,
        execute("rm -f '/cosmos/plugins/mytool.gem' 2>&1"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.remove("mytool", "/cosmos"));
}

TEST_F(PluginServiceTest, PluginNameWithMetacharactersIsNeutralised)
{
    // A malicious plugin name must stay inside a single quoted argument.
    EXPECT_CALL(mock_,
        execute("rm -f '/cosmos/plugins/x'\\''; rm -rf /; '\\''.gem' 2>&1"))
        .WillOnce(Return(ExecutorResult::ok()));

    EXPECT_TRUE(sut_.remove("x'; rm -rf /; '", "/cosmos"));
}

TEST_F(PluginServiceTest, BuildQuotesPluginRoot)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("cd '/cosmos/plugins/my plugin'")))
        .WillOnce(Return(ExecutorResult::ok(
            "Successfully built RubyGem\n./cosmos-my-plugin-0.1.0.gem\n")));

    const auto output = sut_.build("/cosmos/plugins/my plugin");
    EXPECT_THAT(output, HasSubstr("cosmos-my-plugin-0.1.0.gem"));
}
