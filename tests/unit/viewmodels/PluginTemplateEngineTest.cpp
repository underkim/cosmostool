#include "viewmodels/plugin/PluginTemplateEngine.h"

#include <gtest/gtest.h>
#include <QString>

using namespace OpenC3::ViewModels;

TEST(PluginTemplateEngineTest, BuildFilesProducesExpectedTree)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "My description", 0);

    // Top-level plugin scaffolding.
    EXPECT_TRUE(files.contains("plugin.txt"));
    EXPECT_TRUE(files.contains("cosmos-my-plugin.gemspec"));
    EXPECT_TRUE(files.contains("Gemfile"));

    // Target files use the upper-cased target name in the path.
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_cmds.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_tlm.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/screens/fsw.txt"));
    EXPECT_TRUE(files.contains("targets/FSW/procedures/fsw_check.rb"));
}

TEST(PluginTemplateEngineTest, PluginTxtSubstitutesNamesAndDescription)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "My description", 0);

    const QString plugin = files.value("plugin.txt");
    EXPECT_TRUE(plugin.contains("My description"));
    EXPECT_TRUE(plugin.contains("TARGET FSW"));
    // Hyphen in the plugin name is normalised to underscore for the variable.
    EXPECT_TRUE(plugin.contains("VARIABLE my_plugin_target_name"));
}

TEST(PluginTemplateEngineTest, GemspecCarriesGemName)
{
    const auto files =
        PluginTemplateEngine::buildFiles("my-plugin", "fsw", "desc", 0);

    const QString gemspec = files.value("cosmos-my-plugin.gemspec");
    EXPECT_TRUE(gemspec.contains("s.name        = 'cosmos-my-plugin'"));
}

TEST(PluginTemplateEngineTest, CcsdsTemplateDiffersFromGeneric)
{
    const auto generic = PluginTemplateEngine::buildFiles("p", "sat", "d", 0);
    const auto ccsds   = PluginTemplateEngine::buildFiles("p", "sat", "d", 2);

    // Template type 2 (GSE) uses a TCP/IP server interface.
    EXPECT_TRUE(ccsds.value("plugin.txt").contains("tcpip_server_interface"));
    EXPECT_TRUE(generic.value("plugin.txt").contains("tcpip_client_interface"));
}

TEST(PluginTemplateEngineTest, BuildTargetFilesOnlyEmitsTargetTree)
{
    const auto files = PluginTemplateEngine::buildTargetFiles("fsw", 0);

    // No top-level plugin files when adding a target to an existing plugin.
    EXPECT_FALSE(files.contains("plugin.txt"));
    EXPECT_FALSE(files.contains("Gemfile"));
    EXPECT_TRUE(files.contains("targets/FSW/cmd_tlm/fsw_cmds.txt"));
}
