#include "services/plugin/CosmosPluginService.h"
#include "core/api/CosmosRoutes.h"
#include "mocks/MockCosmosApiClient.h"

#include <gtest/gtest.h>

using namespace OpenC3;
using Core::Api::ApiResponse;
using Services::CosmosPluginService;
using Tests::MockCosmosApiClient;
using ::testing::_;
using ::testing::Return;

namespace {

// ── listInstalled ─────────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, ListUsesTheAllRouteNotThePlainIndex)
{
    // GET /plugins (index) renders only `@model_class.names(...)`, a bare
    // array of strings with no version or description. The 'all' member route
    // is the one that returns full model hashes.
    MockCosmosApiClient api;
    EXPECT_CALL(api, get("/openc3-api/plugins/all", _))
        .WillOnce(Return(ApiResponse::ok("{}")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.listInstalled().empty());
}

TEST(CosmosPluginServiceTest, ParsesInstalledPluginsKeyedByInstanceName)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _)).WillOnce(Return(ApiResponse::ok(R"({
        "openc3-cosmos-demo-1.0.0.gem__20240101120000": {
            "name": "openc3-cosmos-demo-1.0.0.gem__20240101120000",
            "description": "Demo plugin"
        }
    })")));

    CosmosPluginService service(api);
    const auto plugins = service.listInstalled();

    ASSERT_EQ(plugins.size(), 1U);
    EXPECT_EQ(plugins[0].name,
              "openc3-cosmos-demo-1.0.0.gem__20240101120000");
    EXPECT_EQ(plugins[0].description, "Demo plugin");
    EXPECT_EQ(plugins[0].status, Models::PluginStatus::Installed);
}

TEST(CosmosPluginServiceTest, FallsBackToTitleWhenDescriptionIsAbsent)
{
    // PluginModel#as_json emits nil for description/title when the gemspec
    // omitted them, so both "missing" and "null" have to be tolerated.
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _)).WillOnce(Return(ApiResponse::ok(R"({
        "demo.gem__1": { "description": null, "title": "Demo Title" }
    })")));

    CosmosPluginService service(api);
    const auto plugins = service.listInstalled();

    ASSERT_EQ(plugins.size(), 1U);
    EXPECT_EQ(plugins[0].description, "Demo Title");
}

TEST(CosmosPluginServiceTest, ListReturnsEmptyOnApiFailureRatherThanThrowing)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(500, R"({"status":"error"})")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.listInstalled().empty());
}

TEST(CosmosPluginServiceTest, ListSurvivesMalformedJson)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _)).WillOnce(Return(ApiResponse::ok("<html>502</html>")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.listInstalled().empty());
}

// ── Phase 1: upload ───────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, UploadPostsFileToPluginsWithFieldNamePlugin)
{
    // plugins#create reads params[:plugin]; any other field name means the
    // controller sees no file and answers "No file received".
    MockCosmosApiClient api;
    EXPECT_CALL(api, postFile("/openc3-api/plugins", "plugin", "/tmp/demo.gem", _, _))
        .WillOnce(Return(ApiResponse::ok(R"({"name":"demo.gem__1"})")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.uploadGem("/tmp/demo.gem"));
}

TEST(CosmosPluginServiceTest, UploadRejectsSuccessResponseWithoutAPluginName)
{
    // A 200 carrying no name would otherwise be forwarded into phase 2 as
    // garbage and fail there with a far less obvious error.
    MockCosmosApiClient api;
    EXPECT_CALL(api, postFile(_, _, _, _, _))
        .WillOnce(Return(ApiResponse::ok(R"({"status":"ok"})")));

    CosmosPluginService service(api);
    const auto result = service.uploadGem("/tmp/not-a-plugin.gem");

    EXPECT_FALSE(result);
    EXPECT_NE(result.message.find("no plugin name"), std::string::npos);
}

TEST(CosmosPluginServiceTest, UploadSurfacesTheCosmosErrorMessage)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, postFile(_, _, _, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(
            400, R"({"status":"error","message":"Gem file is empty"})")));

    CosmosPluginService service(api);
    const auto result = service.uploadGem("/tmp/empty.gem");

    EXPECT_FALSE(result);
    // The COSMOS message, not a bare "HTTP 400", is what the user can act on.
    EXPECT_NE(result.message.find("Gem file is empty"), std::string::npos);
}

// ── Phase 2: install ──────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, InstallUsesInstallRouteWithIdAfterTheInstallSegment)
{
    // routes.rb declares `post '/plugins/install/:id'`, reversing the usual
    // Rails member-route ordering. Getting it backwards yields a 404.
    MockCosmosApiClient api;
    EXPECT_CALL(api, postForm("/openc3-api/plugins/install/demo.gem__1", _, _))
        .WillOnce(Return(ApiResponse::ok(R"("plugin_install_job")")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.installUploaded("demo.gem__1", R"({"name":"demo.gem__1"})"));
}

TEST(CosmosPluginServiceTest, InstallSendsPluginHashAsAFormFieldNotJsonBody)
{
    // plugins#install reads params[:plugin_hash] from Rails params, so a JSON
    // body would arrive as nil and the spawned openc3cli load would get an
    // empty plugin file.
    MockCosmosApiClient api;
    const Core::Api::QueryParams expected{{"plugin_hash", R"({"name":"demo.gem__1"})"}};

    EXPECT_CALL(api, postForm(_, expected, _))
        .WillOnce(Return(ApiResponse::ok(R"("job")")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.installUploaded("demo.gem__1", R"({"name":"demo.gem__1"})"));
}

TEST(CosmosPluginServiceTest, InstallReportsJobStartedRatherThanCompleted)
{
    // The controller renders the spawned process name and returns immediately;
    // it never waits for the install to finish. The message must not claim it
    // did.
    MockCosmosApiClient api;
    EXPECT_CALL(api, postForm(_, _, _))
        .WillOnce(Return(ApiResponse::ok(R"("plugin_install_1")")));

    CosmosPluginService service(api);
    const auto result = service.installUploaded("demo.gem__1", "{}");

    ASSERT_TRUE(result);
    EXPECT_EQ(result.processName, "plugin_install_1");
    EXPECT_NE(result.message.find("started"), std::string::npos);
}

// ── Both phases ───────────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, InstallGemRunsUploadThenInstallUsingTheReturnedName)
{
    // Phase 2's :id must be the suffixed name COSMOS assigned in phase 1.
    // Re-deriving it from the local filename would produce a different string
    // and 404.
    MockCosmosApiClient api;

    EXPECT_CALL(api, postFile(_, _, "/tmp/demo.gem", _, _))
        .WillOnce(Return(ApiResponse::ok(
            R"({"name":"openc3-cosmos-demo-1.0.0.gem__20240101120000"})")));

    EXPECT_CALL(api,
                postForm("/openc3-api/plugins/install/"
                         "openc3-cosmos-demo-1.0.0.gem__20240101120000",
                         _, _))
        .WillOnce(Return(ApiResponse::ok(R"("job")")));

    CosmosPluginService service(api);
    EXPECT_TRUE(service.installGem("/tmp/demo.gem"));
}

TEST(CosmosPluginServiceTest, InstallGemDoesNotAttemptPhaseTwoWhenUploadFails)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, postFile(_, _, _, _, _))
        .WillOnce(Return(ApiResponse::fail("Connection refused")));
    EXPECT_CALL(api, postForm(_, _, _)).Times(0);

    CosmosPluginService service(api);
    EXPECT_FALSE(service.installGem("/tmp/demo.gem"));
}

// ── Uninstall ─────────────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, UninstallDeletesByInstanceName)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, del("/openc3-api/plugins/demo.gem__1", _))
        .WillOnce(Return(ApiResponse::ok(R"("plugin_uninstall_1")")));

    CosmosPluginService service(api);
    const auto result = service.uninstall("demo.gem__1");

    ASSERT_TRUE(result);
    EXPECT_EQ(result.processName, "plugin_uninstall_1");
}

TEST(CosmosPluginServiceTest, UninstallFailureIsReportedNotSwallowed)
{
    // The old shell implementation used `rm -f`, which exits 0 whether or not
    // the target existed - so a mistyped name looked like success. An API
    // error must never be reported as a successful removal.
    MockCosmosApiClient api;
    EXPECT_CALL(api, del(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(
            500, R"({"status":"error","message":"Plugin not found"})")));

    CosmosPluginService service(api);
    const auto result = service.uninstall("nope__1");

    EXPECT_FALSE(result);
    EXPECT_NE(result.message.find("Plugin not found"), std::string::npos);
}

TEST(CosmosPluginServiceTest, JobStatusReadsTheExactProcessModel)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get("/openc3-api/process_status/plugin_install__42", _))
        .WillOnce(Return(ApiResponse::fromHttp(200, R"({
            "name":"plugin_install__42",
            "state":"Running",
            "detail":"demo.gem__0",
            "output":"loading"
        })")));

    CosmosPluginService service(api);
    const auto status = service.jobStatus("plugin_install__42");

    EXPECT_TRUE(status.querySucceeded);
    EXPECT_TRUE(status.found);
    EXPECT_EQ(status.state, "Running");
    EXPECT_EQ(status.detail, "demo.gem__0");
    EXPECT_FALSE(status.isTerminal());
}

TEST(CosmosPluginServiceTest, JobStatusClassifiesTerminalFailure)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(200, R"({
            "name":"plugin_install__43",
            "state":"Error",
            "detail":"demo.gem__0",
            "output":"ERROR: invalid plugin.txt"
        })")));

    CosmosPluginService service(api);
    const auto status = service.jobStatus("plugin_install__43");

    EXPECT_TRUE(status.isTerminal());
    EXPECT_FALSE(status.completedSuccessfully());
    EXPECT_EQ(status.output, "ERROR: invalid plugin.txt");
}

TEST(CosmosPluginServiceTest, JobStatusAllowsProcessModelCreationDelay)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(200, "null")));

    CosmosPluginService service(api);
    const auto status = service.jobStatus("plugin_install__44");

    EXPECT_TRUE(status.querySucceeded);
    EXPECT_FALSE(status.found);
    EXPECT_FALSE(status.isTerminal());
}

// ── Availability ──────────────────────────────────────────────────────────────

TEST(CosmosPluginServiceTest, AvailabilityTracksTheApiClient)
{
    // Lets the UI distinguish "nothing is installed" from "we cannot see
    // COSMOS at all" instead of showing a misleading empty list.
    MockCosmosApiClient api;
    EXPECT_CALL(api, isConnected())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    CosmosPluginService service(api);
    EXPECT_FALSE(service.isAvailable());
    EXPECT_TRUE(service.isAvailable());
}

} // namespace
