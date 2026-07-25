#include "services/script/CosmosScriptService.h"
#include "core/api/CosmosRoutes.h"
#include "mocks/MockCosmosApiClient.h"

#include <gtest/gtest.h>

using namespace OpenC3;
using Core::Api::ApiResponse;
using Services::CosmosScriptService;
using Tests::MockCosmosApiClient;
using ::testing::_;
using ::testing::Return;

namespace {

// ── listScripts ──────────────────────────────────────────────────────────────

TEST(CosmosScriptServiceTest, ListScriptsParsesTheBareArray)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get("/script-api/scripts", _))
        .WillOnce(Return(ApiResponse::ok(
            R"(["INST/procedures/checkout.rb","INST/procedures/collect.rb"])")));

    CosmosScriptService service(api);
    const auto scripts = service.listScripts();

    ASSERT_EQ(scripts.size(), 2U);
    EXPECT_EQ(scripts[0].path, "INST/procedures/checkout.rb");
    EXPECT_EQ(scripts[1].path, "INST/procedures/collect.rb");
}

TEST(CosmosScriptServiceTest, ListScriptsPassesTargetFilterAsQueryParam)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get("/script-api/scripts",
        ::testing::ElementsAre(std::make_pair(std::string("target"), std::string("INST")))))
        .WillOnce(Return(ApiResponse::ok("[]")));

    CosmosScriptService service(api);
    EXPECT_TRUE(service.listScripts("INST").empty());
}

TEST(CosmosScriptServiceTest, ListScriptsReturnsEmptyOnApiFailure)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(401, R"({"message":"nope"})")));

    CosmosScriptService service(api);
    EXPECT_TRUE(service.listScripts().empty());
}

// ── run ───────────────────────────────────────────────────────────────────────

TEST(CosmosScriptServiceTest, RunUsesTheDisconnectedRoute)
{
    // /run (connected mode) needs an ActionCable prompt channel this
    // desktop client does not implement - /run/disconnect is the only mode
    // that cannot silently hang forever on an interactive prompt.
    MockCosmosApiClient api;
    EXPECT_CALL(api, post("/script-api/scripts/INST/procedures/checkout.rb/run/disconnect", _, _))
        .WillOnce(Return(ApiResponse::ok("42")));

    CosmosScriptService service(api);
    const auto result = service.run("INST/procedures/checkout.rb");

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.runningScriptId, "42");
}

TEST(CosmosScriptServiceTest, RunTrimsWhitespaceFromThePlainTextBody)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post(_, _, _))
        .WillOnce(Return(ApiResponse::ok("  42\n")));

    CosmosScriptService service(api);
    const auto result = service.run("INST/procedures/checkout.rb");

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.runningScriptId, "42");
}

TEST(CosmosScriptServiceTest, RunFailsCleanlyWithoutAScriptPath)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post(_, _, _)).Times(0);

    CosmosScriptService service(api);
    const auto result = service.run("");

    EXPECT_FALSE(result.success);
}

TEST(CosmosScriptServiceTest, RunFailureIsReportedNotSwallowed)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(403, R"({"message":"Forbidden"})")));

    CosmosScriptService service(api);
    const auto result = service.run("INST/procedures/checkout.rb");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("Forbidden"), std::string::npos);
}

// ── status ────────────────────────────────────────────────────────────────────

TEST(CosmosScriptServiceTest, StatusReadsRunningState)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get("/script-api/running-script/42", _))
        .WillOnce(Return(ApiResponse::ok(R"({
            "name": "42",
            "state": "running",
            "filename": "INST/procedures/checkout.rb",
            "current_filename": "INST/procedures/checkout.rb",
            "line_no": 5
        })")));

    CosmosScriptService service(api);
    const auto status = service.status("42");

    EXPECT_TRUE(status.querySucceeded);
    EXPECT_TRUE(status.found);
    EXPECT_EQ(status.state, "running");
    EXPECT_EQ(status.lineNo, 5);
    EXPECT_FALSE(status.isTerminal());
}

TEST(CosmosScriptServiceTest, StatusClassifiesTerminalCompletion)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::ok(R"({"name":"42","state":"completed"})")));

    CosmosScriptService service(api);
    const auto status = service.status("42");

    EXPECT_TRUE(status.isTerminal());
    EXPECT_TRUE(status.completedSuccessfully());
}

TEST(CosmosScriptServiceTest, StatusClassifiesTerminalFailureDistinctFromSuccess)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::ok(
            R"({"name":"42","state":"completed_errors","errors":"boom"})")));

    CosmosScriptService service(api);
    const auto status = service.status("42");

    EXPECT_TRUE(status.isTerminal());
    EXPECT_FALSE(status.completedSuccessfully());
    EXPECT_EQ(status.output, "boom");
}

TEST(CosmosScriptServiceTest, StatusTreatsA404AsGoneNotAsAnError)
{
    // running_script#show renders head :not_found once ScriptStatusModel has
    // expired - that must not be reported the same way as a real transport
    // or auth failure.
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _))
        .WillOnce(Return(ApiResponse::fromHttp(404, "")));

    CosmosScriptService service(api);
    const auto status = service.status("42");

    EXPECT_TRUE(status.querySucceeded);
    EXPECT_FALSE(status.found);
}

TEST(CosmosScriptServiceTest, StatusWithoutAnIdFailsCleanly)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, get(_, _)).Times(0);

    CosmosScriptService service(api);
    const auto status = service.status("");

    EXPECT_FALSE(status.querySucceeded);
}

// ── lifecycle actions ────────────────────────────────────────────────────────

TEST(CosmosScriptServiceTest, StopUsesTheStopRoute)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post("/script-api/running-script/42/stop", _, _))
        .WillOnce(Return(ApiResponse::ok()));

    CosmosScriptService service(api);
    EXPECT_TRUE(service.stop("42"));
}

TEST(CosmosScriptServiceTest, PauseUsesThePauseRoute)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post("/script-api/running-script/42/pause", _, _))
        .WillOnce(Return(ApiResponse::ok()));

    CosmosScriptService service(api);
    EXPECT_TRUE(service.pause("42"));
}

TEST(CosmosScriptServiceTest, ResumeUsesTheGoRoute)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, post("/script-api/running-script/42/go", _, _))
        .WillOnce(Return(ApiResponse::ok()));

    CosmosScriptService service(api);
    EXPECT_TRUE(service.resume("42"));
}

// ── availability ─────────────────────────────────────────────────────────────

TEST(CosmosScriptServiceTest, AvailabilityTracksTheApiClient)
{
    MockCosmosApiClient api;
    EXPECT_CALL(api, isConnected())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    CosmosScriptService service(api);
    EXPECT_FALSE(service.isAvailable());
    EXPECT_TRUE(service.isAvailable());
}

} // namespace
