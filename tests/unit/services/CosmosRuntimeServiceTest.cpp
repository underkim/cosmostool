#include "services/runtime/CosmosRuntimeService.h"
#include "core/api/CosmosRoutes.h"
#include "mocks/MockCosmosApiClient.h"

#include <gtest/gtest.h>

using namespace OpenC3;
using Core::Api::ApiConfig;
using Core::Api::ApiResponse;
using Services::CosmosRuntimeService;
using Tests::MockCosmosApiClient;
using ::testing::_;
using ::testing::Return;

namespace {

// A JSON-RPC 2.0 success envelope, matching what json_drb.rb#process_request
// renders for a successful call: {"jsonrpc":"2.0","id":N,"result":...}.
std::string jsonRpcSuccess(const std::string& resultJson, int id = 1)
{
    return R"({"jsonrpc":"2.0","id":)" + std::to_string(id)
         + R"(,"result":)" + resultJson + "}";
}

std::string jsonRpcError(const std::string& message, int code = -1, int id = 1)
{
    return R"({"jsonrpc":"2.0","id":)" + std::to_string(id)
         + R"(,"error":{"code":)" + std::to_string(code)
         + R"(,"message":")" + message + "\"}}";
}

class CosmosRuntimeServiceTest : public ::testing::Test {
protected:
    MockCosmosApiClient api_;
};

// ── listTargets ──────────────────────────────────────────────────────────────

TEST_F(CosmosRuntimeServiceTest, ListTargetsCallsGetTargetNamesOverTheDrbEndpoint)
{
    ApiConfig config;
    config.scope = "DEFAULT";
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"get_target_names\""), std::string::npos);
            EXPECT_NE(body.find("\"scope\":\"DEFAULT\""), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(R"(["INST","EXAMPLE"])"));
        });

    CosmosRuntimeService service(api_);
    const auto targets = service.listTargets();

    ASSERT_EQ(targets.size(), 2U);
    EXPECT_EQ(targets[0].name, "INST");
    EXPECT_EQ(targets[1].name, "EXAMPLE");
}

TEST_F(CosmosRuntimeServiceTest, ListTargetsReturnsEmptyOnJsonRpcError)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(401,
            jsonRpcError("Authentication error", -32500))));

    CosmosRuntimeService service(api_);
    EXPECT_TRUE(service.listTargets().empty());
}

TEST_F(CosmosRuntimeServiceTest, ListTargetsReturnsEmptyOnTransportFailure)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fail("Connection refused")));

    CosmosRuntimeService service(api_);
    EXPECT_TRUE(service.listTargets().empty());
}

// ── listCommands / listTelemetry ────────────────────────────────────────────

TEST_F(CosmosRuntimeServiceTest, ListCommandsCallsGetAllCmdNamesWithTheTarget)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"get_all_cmd_names\""), std::string::npos);
            EXPECT_NE(body.find("\"params\":[\"INST\"]"), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(R"(["COLLECT","ABORT"])"));
        });

    CosmosRuntimeService service(api_);
    const auto commands = service.listCommands("INST");

    ASSERT_EQ(commands.size(), 2U);
    EXPECT_EQ(commands[0].targetName, "INST");
    EXPECT_EQ(commands[0].packetName, "COLLECT");
    EXPECT_EQ(commands[1].packetName, "ABORT");
}

TEST_F(CosmosRuntimeServiceTest, ListTelemetryCallsGetAllTlmNamesWithTheTarget)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"get_all_tlm_names\""), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(R"(["HEALTH_STATUS","ADCS"])"));
        });

    CosmosRuntimeService service(api_);
    const auto packets = service.listTelemetry("INST");

    ASSERT_EQ(packets.size(), 2U);
    EXPECT_EQ(packets[0].packetName, "HEALTH_STATUS");
}

TEST_F(CosmosRuntimeServiceTest, ListCommandsWithEmptyTargetMakesNoCall)
{
    EXPECT_CALL(api_, post(_, _, _)).Times(0);

    CosmosRuntimeService service(api_);
    EXPECT_TRUE(service.listCommands("").empty());
}

// ── telemetryValues ──────────────────────────────────────────────────────────

TEST_F(CosmosRuntimeServiceTest, TelemetryValuesParsesItemValueLimitsTriples)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"get_tlm_packet\""), std::string::npos);
            EXPECT_NE(body.find("\"params\":[\"INST\",\"HEALTH_STATUS\"]"), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(
                R"([["TEMP1",25.5,"GREEN"],["TEMP2",-999,"RED_LOW"]])"));
        });

    CosmosRuntimeService service(api_);
    const auto values = service.telemetryValues("INST", "HEALTH_STATUS");

    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0].itemName, "TEMP1");
    EXPECT_EQ(values[0].limitsState, "GREEN");
    EXPECT_EQ(values[1].itemName, "TEMP2");
    EXPECT_EQ(values[1].limitsState, "RED_LOW");
}

TEST_F(CosmosRuntimeServiceTest, TelemetryValuesToleratesAMissingLimitsState)
{
    ApiConfig config;
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::ok(jsonRpcSuccess(R"([["TEMP1",25.5,null]])"))));

    CosmosRuntimeService service(api_);
    const auto values = service.telemetryValues("INST", "HEALTH_STATUS");

    ASSERT_EQ(values.size(), 1U);
    EXPECT_TRUE(values[0].limitsState.empty());
}

TEST_F(CosmosRuntimeServiceTest, TelemetryValuesWithEmptyArgsMakesNoCall)
{
    EXPECT_CALL(api_, post(_, _, _)).Times(0);

    CosmosRuntimeService service(api_);
    EXPECT_TRUE(service.telemetryValues("", "").empty());
    EXPECT_TRUE(service.telemetryValues("INST", "").empty());
}

// ── availability ─────────────────────────────────────────────────────────────

TEST_F(CosmosRuntimeServiceTest, AvailabilityTracksTheApiClient)
{
    EXPECT_CALL(api_, isConnected())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    CosmosRuntimeService service(api_);
    EXPECT_FALSE(service.isAvailable());
    EXPECT_TRUE(service.isAvailable());
}

} // namespace
