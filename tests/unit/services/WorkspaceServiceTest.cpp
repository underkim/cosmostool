#include "services/workspace/WorkspaceService.h"
#include "mocks/MockCommandExecutor.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class WorkspaceServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor mock_;
    WorkspaceService    sut_{mock_};
};

TEST_F(WorkspaceServiceTest, ListSourcesScansPluginFolders)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-my-plugin\t/cosmos/plugins/cosmos-my-plugin\t"
            "/cosmos/plugins/cosmos-my-plugin/plugin.txt\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin-0.1.0.gem|FSW,ADCS,\n")));
    EXPECT_CALL(mock_,
        execute(HasSubstr("cat '/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec'")))
        .WillOnce(Return(ExecutorResult::ok(
            "Gem::Specification.new do |s|\n"
            "  s.name        = 'cosmos-my-plugin'\n"
            "  s.version     = '1.2.3'\n"
            "  s.summary     = 'A test plugin'\n"
            "  s.description = 'A test plugin'\n"
            "  s.authors     = ['Jane Doe']\n"
            "end\n")));

    const auto plugins = sut_.listSources("/cosmos");
    ASSERT_EQ(plugins.size(), 1u);
    EXPECT_EQ(plugins[0].name, "cosmos-my-plugin");
    EXPECT_EQ(plugins[0].rootPath, "/cosmos/plugins/cosmos-my-plugin");
    EXPECT_EQ(plugins[0].pluginTxtPath, "/cosmos/plugins/cosmos-my-plugin/plugin.txt");
    EXPECT_EQ(plugins[0].gemspecPath, "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec");
    EXPECT_EQ(plugins[0].targets.size(), 2u);
    EXPECT_EQ(plugins[0].version, "1.2.3");
    EXPECT_EQ(plugins[0].description, "A test plugin");
    EXPECT_EQ(plugins[0].author, "Jane Doe");
}

// A description containing an escaped apostrophe (the same escaping
// PluginTemplateEngine applies - see the earlier apostrophe-escaping fix)
// must round-trip back to its original, unescaped form for display, not
// truncate at the first \' or keep the backslash in the displayed text.
TEST_F(WorkspaceServiceTest, ListSourcesUnescapesApostropheInDescription)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-my-plugin\t/cosmos/plugins/cosmos-my-plugin\t"
            "/cosmos/plugins/cosmos-my-plugin/plugin.txt\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin-0.1.0.gem|\n")));
    EXPECT_CALL(mock_,
        execute(HasSubstr("cat '/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec'")))
        .WillOnce(Return(ExecutorResult::ok(
            "Gem::Specification.new do |s|\n"
            "  s.version     = '0.0.1'\n"
            "  s.description = 'Team\\'s satellite plugin'\n"
            "end\n")));

    const auto plugins = sut_.listSources("/cosmos");
    ASSERT_EQ(plugins.size(), 1u);
    EXPECT_EQ(plugins[0].description, "Team's satellite plugin");
}

TEST_F(WorkspaceServiceTest, ListSourcesLeavesMetadataEmptyWhenGemspecUnreadable)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-my-plugin\t/cosmos/plugins/cosmos-my-plugin\t"
            "/cosmos/plugins/cosmos-my-plugin/plugin.txt\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec\t"
            "/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin-0.1.0.gem|\n")));
    EXPECT_CALL(mock_,
        execute(HasSubstr("cat '/cosmos/plugins/cosmos-my-plugin/cosmos-my-plugin.gemspec'")))
        .WillOnce(Return(ExecutorResult::fail("not found")));

    const auto plugins = sut_.listSources("/cosmos");
    ASSERT_EQ(plugins.size(), 1u);
    EXPECT_TRUE(plugins[0].version.empty());
    EXPECT_TRUE(plugins[0].description.empty());
    EXPECT_TRUE(plugins[0].author.empty());
}

TEST_F(WorkspaceServiceTest, ListSourcesAcceptsOpenC3ScriptPath)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("plugins_dir='/cosmos/plugins'")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto plugins = sut_.listSources("/cosmos/openc3.sh");
    EXPECT_TRUE(plugins.empty());
}

TEST_F(WorkspaceServiceTest, ValidateQuotesLocalPath)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("p='/tmp/my plugin'")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto result = sut_.validate("/tmp/my plugin");
    EXPECT_TRUE(result.valid);
}

TEST_F(WorkspaceServiceTest, ValidateReportsMissingPluginTxt)
{
    EXPECT_CALL(mock_, execute(_))
        .WillOnce(Return(ExecutorResult::ok("MISSING_PLUGIN_TXT\n")));

    const auto result = sut_.validate("/tmp/plugin");
    EXPECT_FALSE(result.valid);
    ASSERT_EQ(result.issues.size(), 1u);
    EXPECT_EQ(result.issues[0].code, "MISSING_PLUGIN_TXT");
}

TEST_F(WorkspaceServiceTest, BackupQuotesConstructedPath)
{
    // install()/remove() used to live here, exercising `cp` into
    // <root>/plugins/ and `rm -f`. Both were deleted rather than moved: they
    // manipulated files on the host and reported success while having no
    // effect on a running COSMOS. Installation now goes through
    // CosmosPluginService (see CosmosPluginServiceTest). backup() is the
    // remaining path that builds a plugins/ path from a caller-supplied name,
    // so the shell-quoting guarantee is asserted here instead.
    EXPECT_CALL(mock_, downloadFile("/cosmos/plugins/mytool.gem", "/tmp/out.gem"))
        .WillOnce(Return(true));

    EXPECT_TRUE(sut_.backup("mytool", "/tmp/out.gem", "/cosmos"));
}

TEST_F(WorkspaceServiceTest, PluginRootWithMetacharactersIsNeutralised)
{
    // A malicious path must stay inside a single quoted argument rather than
    // terminating it and injecting a second command.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr(
        "cd '/tmp/x'\\''; rm -rf /; '\\'''")))
        .WillOnce(Return(ExecutorResult::ok("out.gem\n")));

    (void)sut_.build("/tmp/x'; rm -rf /; '");
}

TEST_F(WorkspaceServiceTest, BuildQuotesPluginRoot)
{
    EXPECT_CALL(mock_,
        execute(HasSubstr("cd '/cosmos/plugins/my plugin'")))
        .WillOnce(Return(ExecutorResult::ok(
            "Successfully built RubyGem\n./cosmos-my-plugin-0.1.0.gem\n")));

    const auto output = sut_.build("/cosmos/plugins/my plugin");
    EXPECT_THAT(output, HasSubstr("cosmos-my-plugin-0.1.0.gem"));
}

// PluginViewModel::build() has no separate success flag - it decides
// success/failure by sniffing this returned string for "ERROR"/"failed".
// A failed build (missing `gem` binary, missing .gemspec, or gem build
// itself erroring) must therefore both reach the caller with real content
// and say "ERROR" - not come back empty, and not come back as the generic
// "Process exited with code N" text with no such keyword, which the
// heuristic would misread as a successful build (see ExecutorResult::
// fromProcess()'s errorMessage fallback, which build() used to return here).
TEST_F(WorkspaceServiceTest, BuildSurfacesFailureTextInsteadOfGenericMessage)
{
    EXPECT_CALL(mock_, execute(HasSubstr("cd '/cosmos/plugins/broken'")))
        .WillOnce(Return(ExecutorResult::fromProcess(
            1, "ERROR: No .gemspec file found in the plugin folder.\n", "")));

    const auto output = sut_.build("/cosmos/plugins/broken");
    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("No .gemspec file found"));
}

TEST_F(WorkspaceServiceTest, BuildArtifactReturnsTheExactRemoteGemPath)
{
    EXPECT_CALL(mock_, execute(HasSubstr("OPENC3_ARTIFACT")))
        .WillOnce(Return(ExecutorResult::ok(
            "Successfully built RubyGem\n"
            "OPENC3_ARTIFACT=/cosmos/plugins/demo/openc3-cosmos-demo-1.0.0.gem\n")));

    const auto result = sut_.buildArtifact("/cosmos/plugins/demo");

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.remoteGemPath,
              "/cosmos/plugins/demo/openc3-cosmos-demo-1.0.0.gem");
    EXPECT_THAT(result.output, HasSubstr("Successfully built"));
}

TEST_F(WorkspaceServiceTest, BuildArtifactFailsWithoutAnArtifactMarker)
{
    EXPECT_CALL(mock_, execute(_))
        .WillOnce(Return(ExecutorResult::ok("Successfully built RubyGem\n")));

    const auto result = sut_.buildArtifact("/cosmos/plugins/demo");

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.output, HasSubstr("without reporting an artifact"));
}

TEST_F(WorkspaceServiceTest, DownloadArtifactDelegatesExactPathsToExecutor)
{
    EXPECT_CALL(mock_, downloadFile(
        "/remote/openc3-cosmos-demo.gem", "C:/temp/openc3-cosmos-demo.gem"))
        .WillOnce(Return(true));

    EXPECT_TRUE(sut_.downloadArtifact(
        "/remote/openc3-cosmos-demo.gem", "C:/temp/openc3-cosmos-demo.gem"));
}
