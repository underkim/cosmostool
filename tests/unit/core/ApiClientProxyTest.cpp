#include "core/api/ApiClientProxy.h"
#include "core/api/CosmosRoutes.h"
#include "mocks/MockCosmosApiClient.h"

#include <gtest/gtest.h>

using namespace OpenC3::Core::Api;
using OpenC3::Tests::MockCosmosApiClient;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

// ── Null-object behaviour ─────────────────────────────────────────────────────

TEST(ApiClientProxyTest, DefaultsToNullClientAndReportsDisconnected)
{
    ApiClientProxy proxy;
    EXPECT_FALSE(proxy.isConnected());
    EXPECT_FALSE(proxy.connect());
}

TEST(ApiClientProxyTest, EveryRequestFailsSafelyBeforeSwap)
{
    // The point of the null object: services may be constructed and called
    // before any COSMOS endpoint exists, and must get a clean failure rather
    // than crashing or needing a null check.
    ApiClientProxy proxy;

    EXPECT_FALSE(proxy.get("/openc3-api/plugins", {}));
    EXPECT_FALSE(proxy.post("/openc3-api/plugins", "{}", {}));
    EXPECT_FALSE(proxy.postForm("/openc3-api/plugins", {}, {}));
    EXPECT_FALSE(proxy.postFile("/openc3-api/plugins", "plugin", "x.gem", {}, {}));
    EXPECT_FALSE(proxy.del("/openc3-api/plugins/x", {}));

    EXPECT_FALSE(proxy.get("/openc3-api/plugins", {}).errorMessage.empty());
}

// ── Delegation ────────────────────────────────────────────────────────────────

TEST(ApiClientProxyTest, ForwardsToSwappedClient)
{
    ApiClientProxy       proxy;
    MockCosmosApiClient  client;

    EXPECT_CALL(client, get("/openc3-api/scopes", _))
        .WillOnce(Return(ApiResponse::ok(R"(["DEFAULT"])")));

    proxy.swap(&client);

    auto response = proxy.get(Routes::kScopes, {});
    EXPECT_TRUE(response);
    EXPECT_EQ(response.body, R"(["DEFAULT"])");
}

TEST(ApiClientProxyTest, ForwardsPostBodyAndParams)
{
    ApiClientProxy      proxy;
    MockCosmosApiClient client;
    proxy.swap(&client);

    const QueryParams params{{"id", "PLUG"}};
    EXPECT_CALL(client, post("/openc3-api/api", "{\"method\":\"tlm\"}", params))
        .WillOnce(Return(ApiResponse::ok("{}")));

    EXPECT_TRUE(proxy.post("/openc3-api/api", "{\"method\":\"tlm\"}", params));
}

TEST(ApiClientProxyTest, ForwardsFileUploadArguments)
{
    ApiClientProxy      proxy;
    MockCosmosApiClient client;
    proxy.swap(&client);

    EXPECT_CALL(client,
                postFile("/openc3-api/plugins", "plugin", "/tmp/a.gem", _, _))
        .WillOnce(Return(ApiResponse::ok("{\"name\":\"a.gem\"}")));

    auto response =
        proxy.postFile("/openc3-api/plugins", "plugin", "/tmp/a.gem", {}, {});
    EXPECT_TRUE(response);
}

TEST(ApiClientProxyTest, ResetRestoresNullClient)
{
    ApiClientProxy      proxy;
    MockCosmosApiClient client;

    EXPECT_CALL(client, isConnected()).WillRepeatedly(Return(true));

    proxy.swap(&client);
    EXPECT_TRUE(proxy.isConnected());

    proxy.reset();
    EXPECT_FALSE(proxy.isConnected());
    EXPECT_FALSE(proxy.get("/openc3-api/scopes", {}));
}

TEST(ApiClientProxyTest, SwapWithNullptrFallsBackToNullClient)
{
    // Guards the same hazard ExecutorProxy handles: a caller passing a null
    // pointer must not leave the proxy dereferencing it.
    ApiClientProxy proxy;
    proxy.swap(nullptr);
    EXPECT_FALSE(proxy.isConnected());
    EXPECT_FALSE(proxy.get("/openc3-api/scopes", {}));
}

// ── ApiResponse semantics ─────────────────────────────────────────────────────

TEST(ApiResponseTest, TwoHundredRangeIsSuccess)
{
    EXPECT_TRUE(ApiResponse::fromHttp(200, "{}"));
    EXPECT_TRUE(ApiResponse::fromHttp(201, "{}"));
    EXPECT_TRUE(ApiResponse::fromHttp(299, "{}"));
}

TEST(ApiResponseTest, NonTwoHundredIsFailureButKeepsBody)
{
    // COSMOS reports errors as {"status":"error","message":"..."} alongside a
    // 4xx/5xx, and that message is the only actionable diagnostic - so a
    // failed response must not discard its body.
    const auto response = ApiResponse::fromHttp(
        500, R"({"status":"error","message":"boom"})");

    EXPECT_FALSE(response);
    EXPECT_EQ(response.statusCode, 500);
    EXPECT_EQ(response.body, R"({"status":"error","message":"boom"})");
    EXPECT_EQ(response.errorMessage, "HTTP 500");
}

TEST(ApiResponseTest, TransportFailureHasNoHttpStatus)
{
    // statusCode 0 is how callers distinguish "COSMOS never answered" from
    // "COSMOS answered with an error".
    const auto response = ApiResponse::fail("Connection refused");
    EXPECT_FALSE(response);
    EXPECT_EQ(response.statusCode, 0);
    EXPECT_EQ(response.errorMessage, "Connection refused");
}

// ── Route construction ────────────────────────────────────────────────────────

TEST(CosmosRoutesTest, PluginInstallPutsIdAfterInstallSegment)
{
    // routes.rb declares `post '/plugins/install/:id'`, which reverses the
    // usual Rails member-route ordering; getting this backwards yields a 404
    // that looks like a missing plugin.
    EXPECT_EQ(Routes::pluginInstall("openc3-cosmos-demo__20240101"),
              "/openc3-api/plugins/install/openc3-cosmos-demo__20240101");
}

TEST(CosmosRoutesTest, ScriptRoutesUseScriptApiPrefix)
{
    // Script Runner is a separate service behind the same traefik port and
    // uses /script-api, not /openc3-api.
    EXPECT_EQ(std::string(Routes::kScriptPing), "/script-api/ping");
    EXPECT_EQ(Routes::scriptRun("INST/procedures/checkout.rb"),
              "/script-api/scripts/INST/procedures/checkout.rb/run");
}

TEST(CosmosRoutesTest, CmdTlmRoutesUseOpenC3ApiPrefix)
{
    EXPECT_EQ(std::string(Routes::kPlugins), "/openc3-api/plugins");
    EXPECT_EQ(std::string(Routes::kScopes), "/openc3-api/scopes");
    EXPECT_EQ(Routes::target("INST"), "/openc3-api/targets/INST");
}

} // namespace
