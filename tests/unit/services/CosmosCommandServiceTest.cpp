#include "services/runtime/CosmosCommandService.h"
#include "core/api/CosmosRoutes.h"
#include "mocks/MockCosmosApiClient.h"

#include <gtest/gtest.h>

using namespace OpenC3;
using Core::Api::ApiConfig;
using Core::Api::ApiResponse;
using Services::CosmosCommandService;
using Services::CommandParameter;
using Tests::MockCosmosApiClient;
using ::testing::_;
using ::testing::Return;

namespace {

std::string jsonRpcSuccess(const std::string& resultJson, int id = 1)
{
    return R"({"jsonrpc":"2.0","id":)" + std::to_string(id)
         + R"(,"result":)" + resultJson + "}";
}

// Mirrors what json_drb.rb's process_request produces for a rescued
// exception: an "error" object whose "data" is Exception#as_json's hash -
// {"class", "message", "backtrace", "instance_variables"}.
std::string jsonRpcExceptionError(
    const std::string& exceptionClass, const std::string& message,
    int code, const std::string& instanceVarsJson = "{}", int id = 1)
{
    return R"({"jsonrpc":"2.0","id":)" + std::to_string(id)
         + R"(,"error":{"code":)" + std::to_string(code)
         + R"(,"message":")" + message
         + R"(","data":{"class":")" + exceptionClass
         + R"(","message":")" + message
         + R"(","instance_variables":)" + instanceVarsJson + "}}}";
}

class CosmosCommandServiceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        config_.scope = "DEFAULT";
        ON_CALL(api_, config()).WillByDefault(::testing::ReturnRef(config_));
    }

    MockCosmosApiClient api_;
    ApiConfig config_;
};

// ── send() success ───────────────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, SendCallsCmdWithTargetCommandAndParams)
{
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"cmd\""), std::string::npos);
            EXPECT_NE(body.find("\"params\":[\"INST\",\"COLLECT\",{\"TYPE\":\"NORMAL\"}]"),
                      std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(
                R"({"cmd_string":"INST COLLECT with TYPE NORMAL"})"));
        });

    CosmosCommandService service(api_);
    const auto result = service.send(
        "INST", "COLLECT", {CommandParameter{"TYPE", "NORMAL"}});

    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.hazardous);
    EXPECT_EQ(result.cmdString, "INST COLLECT with TYPE NORMAL");
}

TEST_F(CosmosCommandServiceTest, SendConvertsNumericLookingValuesToNumbers)
{
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            // 5 (no quotes) and 3.5 (no quotes) - not "5"/"3.5" - matching
            // COSMOS's own text-syntax convention for numeric parameters.
            EXPECT_NE(body.find("\"COUNT\":5"), std::string::npos);
            EXPECT_NE(body.find("\"RATIO\":3.5"), std::string::npos);
            EXPECT_NE(body.find("\"LABEL\":\"abc\""), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(R"({"cmd_string":"ok"})"));
        });

    CosmosCommandService service(api_);
    const auto result = service.send("INST", "COLLECT", {
        {"COUNT", "5"}, {"RATIO", "3.5"}, {"LABEL", "abc"}
    });

    EXPECT_TRUE(result.success);
}

TEST_F(CosmosCommandServiceTest, SendFailsCleanlyWithoutTargetOrCommand)
{
    EXPECT_CALL(api_, post(_, _, _)).Times(0);

    CosmosCommandService service(api_);
    EXPECT_FALSE(service.send("", "COLLECT", {}).success);
    EXPECT_FALSE(service.send("INST", "", {}).success);
}

// ── send() hazardous ─────────────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, SendReportsHazardousConfirmationNotFailure)
{
    // HazardousError -> JsonRpcError::ErrorCode::HAZARDOUS_ERROR (-32502) -
    // must be surfaced as "needs confirmation", not lumped in with a normal
    // failure the caller might silently retry.
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(409, jsonRpcExceptionError(
            "HazardousError", "INST COLLECT with TYPE SPECIAL is Hazardous", -32502,
            R"({"@hazardous_description":"Special collect is destructive"})"))));

    CosmosCommandService service(api_);
    const auto result = service.send(
        "INST", "COLLECT", {CommandParameter{"TYPE", "SPECIAL"}});

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.hazardous);
    EXPECT_EQ(result.hazardousDescription, "Special collect is destructive");
}

TEST_F(CosmosCommandServiceTest, SendHazardousWithoutADescriptionStillFlagsHazardous)
{
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(409, jsonRpcExceptionError(
            "HazardousError", "Hazardous", -32502))));

    CosmosCommandService service(api_);
    const auto result = service.send("INST", "COLLECT", {});

    EXPECT_TRUE(result.hazardous);
    EXPECT_TRUE(result.hazardousDescription.empty());
}

// ── send() disabled ──────────────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, SendReportsDisabledCommandDistinctly)
{
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(500, jsonRpcExceptionError(
            "DisabledError", "INST COLLECT is Disabled", -1))));

    CosmosCommandService service(api_);
    const auto result = service.send("INST", "COLLECT", {});

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.hazardous);
    EXPECT_TRUE(result.disabled);
}

// ── send() generic failure ───────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, SendReportsAGenericFailureAsNeitherHazardousNorDisabled)
{
    EXPECT_CALL(api_, post(_, _, _))
        .WillOnce(Return(ApiResponse::fromHttp(404, jsonRpcExceptionError(
            "NoMethodError", "Method not found", -32601))));

    CosmosCommandService service(api_);
    const auto result = service.send("INST", "BOGUS", {});

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.hazardous);
    EXPECT_FALSE(result.disabled);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ── sendConfirmed() ──────────────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, SendConfirmedUsesCmdNoHazardousCheck)
{
    EXPECT_CALL(api_, post("/openc3-api/api", _, _))
        .WillOnce([](const std::string&, const std::string& body, const auto&) {
            EXPECT_NE(body.find("\"method\":\"cmd_no_hazardous_check\""), std::string::npos);
            return ApiResponse::ok(jsonRpcSuccess(R"({"cmd_string":"ok"})"));
        });

    CosmosCommandService service(api_);
    const auto result = service.sendConfirmed(
        "INST", "COLLECT", {CommandParameter{"TYPE", "SPECIAL"}});

    EXPECT_TRUE(result.success);
}

// ── availability ─────────────────────────────────────────────────────────────

TEST_F(CosmosCommandServiceTest, AvailabilityTracksTheApiClient)
{
    EXPECT_CALL(api_, isConnected())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    CosmosCommandService service(api_);
    EXPECT_FALSE(service.isAvailable());
    EXPECT_TRUE(service.isAvailable());
}

} // namespace
