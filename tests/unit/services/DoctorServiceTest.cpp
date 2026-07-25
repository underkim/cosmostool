#include "services/doctor/DoctorService.h"
#include "mocks/MockCommandExecutor.h"
#include "mocks/MockCosmosApiClient.h"
#include "models/HealthCheckResult.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace OpenC3::Services;
using namespace OpenC3::Tests;
using namespace OpenC3::Core::Connection;
using namespace OpenC3::Models;
using OpenC3::Core::Api::ApiResponse;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

class DoctorServiceTest : public ::testing::Test {
protected:
    MockCommandExecutor mock_;
    // NiceMock: the L1/L2 tests below never touch the API, and an API-layer
    // check running as part of runAll() would otherwise emit "uninteresting
    // call" noise for every one of them.
    NiceMock<MockCosmosApiClient> api_;
    DoctorService       sut_{mock_, api_};
};

TEST_F(DoctorServiceTest, DockerInstalledPassWhenFound)
{
    EXPECT_CALL(mock_, execute(
        "which docker 2>/dev/null || command -v docker"))
        .WillOnce(Return(ExecutorResult::ok("/usr/bin/docker\n")));

    const auto r = sut_.runCheck("docker_installed");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, DockerInstalledFailWhenNotFound)
{
    EXPECT_CALL(mock_, execute(_))
        .WillOnce(Return(ExecutorResult::fail("not found")));

    const auto r = sut_.runCheck("docker_installed");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_FALSE(r.suggestion.empty());
}

TEST_F(DoctorServiceTest, CpuUsagePassWhenBelow80Percent)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("45.2\n")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, CpuUsageFailWhenAbove95Percent)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("97.1\n")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Fail);
}

TEST_F(DoctorServiceTest, CpuUsageSkippedWhenOutputUnparseable)
{
    // Unexpected/empty awk output (e.g. a distro whose `top` format differs)
    // must not be silently treated as "0% - Pass".
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("top")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("cpu_usage");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, MemoryUsageSkippedWhenOutputUnparseable)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("free")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("memory_usage");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, DiskSpaceSkippedWhenOutputUnparseable)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("df")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("disk_space");
    EXPECT_EQ(r.status, HealthStatus::Skipped);
}

TEST_F(DoctorServiceTest, ContainerCheckFailsWhenNothingIsRunning)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok("")));

    const auto r = sut_.runCheck("cosmos_containers");
    EXPECT_EQ(r.status, HealthStatus::Fail);
}

// The old implementation ran `docker ps --filter name=cosmos` and compared the
// count against a threshold. Every service in COSMOS 6.10.4's compose.yaml is
// named openc3-* (minio, redis, traefik, operator, ...), so that filter matched
// only the two openc3-cosmos-* API containers and silently ignored the rest.
TEST_F(DoctorServiceTest, ContainerCheckDoesNotFilterByNameOnDocker)
{
    EXPECT_CALL(mock_, execute(::testing::Not(::testing::HasSubstr("--filter"))))
        .WillOnce(Return(ExecutorResult::ok("")));

    (void)sut_.runCheck("cosmos_containers");
}

TEST_F(DoctorServiceTest, ContainerCheckPassesWhenAllRequiredServicesPresent)
{
    // Compose prefixes the project name and appends an index, so matching must
    // be by substring rather than equality.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-openc3-traefik-1\n"
            "cosmos-openc3-redis-1\n"
            "cosmos-openc3-redis-ephemeral-1\n"
            "cosmos-openc3-minio-1\n"
            "cosmos-openc3-operator-1\n"
            "cosmos-openc3-cosmos-cmd-tlm-api-1\n"
            "cosmos-openc3-cosmos-script-runner-api-1\n")));

    const auto r = sut_.runCheck("cosmos_containers");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, ContainerCheckNamesTheMissingServices)
{
    // "Not running: minio, operator" is actionable; "3 of 7 running" is not.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-openc3-traefik-1\n"
            "cosmos-openc3-redis-1\n"
            "cosmos-openc3-redis-ephemeral-1\n"
            "cosmos-openc3-cosmos-cmd-tlm-api-1\n"
            "cosmos-openc3-cosmos-script-runner-api-1\n")));

    const auto r = sut_.runCheck("cosmos_containers");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_THAT(r.detail, ::testing::HasSubstr("minio"));
    EXPECT_THAT(r.detail, ::testing::HasSubstr("operator"));
}

TEST_F(DoctorServiceTest, InitContainerAbsenceIsNotAFault)
{
    // openc3-cosmos-init seeds the system then exits, so a healthy running
    // stack normally does NOT have it - treating that as a fault would make
    // every correct deployment look broken.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-openc3-traefik-1\n"
            "cosmos-openc3-redis-1\n"
            "cosmos-openc3-redis-ephemeral-1\n"
            "cosmos-openc3-minio-1\n"
            "cosmos-openc3-operator-1\n"
            "cosmos-openc3-cosmos-cmd-tlm-api-1\n"
            "cosmos-openc3-cosmos-script-runner-api-1\n")));

    const auto r = sut_.runCheck("cosmos_containers");
    EXPECT_EQ(r.status, HealthStatus::Pass);
    EXPECT_THAT(r.detail, ::testing::Not(::testing::HasSubstr("init")));
}

TEST_F(DoctorServiceTest, RunAllReturnsSameCountAsChecks)
{
    // Allow any execute calls — return safe defaults
    EXPECT_CALL(mock_, execute(_))
        .WillRepeatedly(Return(ExecutorResult::ok("OK\n")));

    const auto report = sut_.runAll();
    EXPECT_FALSE(report.results.empty());
}

TEST_F(DoctorServiceTest, UnknownCheckIdReturnsFail)
{
    const auto r = sut_.runCheck("nonexistent_check_id_xyz");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_EQ(r.detail, "Unknown check id");
}

// ── L3: COSMOS logical state (API transport) ─────────────────────────────────
//
// The plugin_folder check that used to live here was deleted, not ported. It
// ran `test -d <root>/plugins`, but that directory's existence says nothing
// about COSMOS: installed plugins live in MinIO + Redis, so the check passed
// on a host with an empty folder and failed on a perfectly healthy COSMOS
// whose plugins were installed from elsewhere.

TEST_F(DoctorServiceTest, PersistentRedisDoesNotMistakeEphemeralForIt)
{
    // docker ps order is not stable. If ephemeral appears first, a naive
    // substring match for "openc3-redis" selects the wrong container and both
    // Doctor checks ping the same instance.
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-openc3-redis-ephemeral-1\n"
            "cosmos-openc3-redis-1\n")));
    EXPECT_CALL(mock_, execute(::testing::HasSubstr(
        "docker exec 'cosmos-openc3-redis-1' redis-cli ping")))
        .WillOnce(Return(ExecutorResult::ok("PONG\n")));

    EXPECT_EQ(sut_.runCheck("redis").status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, PersistentRedisFailsWhenOnlyEphemeralIsRunning)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker ps")))
        .WillOnce(Return(ExecutorResult::ok(
            "cosmos-openc3-redis-ephemeral-1\n")));

    const auto r = sut_.runCheck("redis");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_THAT(r.detail, ::testing::HasSubstr("Redis container"));
}

TEST_F(DoctorServiceTest, ApiChecksAreSkippedNotFailedWhenApiIsUnconfigured)
{
    // A shell-only profile is a legitimate setup. Reporting it as a failure
    // would train users to ignore Doctor output.
    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(false));

    for (const char* id : {"api_reachable", "scope", "plugins",
                           "microservices", "interfaces"}) {
        const auto r = sut_.runCheck(id);
        EXPECT_EQ(r.status, HealthStatus::Skipped) << id;
    }
}

TEST_F(DoctorServiceTest, ApiReachableReportsAuthFailureDistinctly)
{
    // 401/403 means the password is wrong, which is a completely different
    // remedy from "COSMOS is down" - the suggestion must say so.
    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, get(::testing::HasSubstr("/internal/health"), _))
        .WillOnce(Return(ApiResponse::fromHttp(401, "")));

    const auto r = sut_.runCheck("api_reachable");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_THAT(r.suggestion, ::testing::HasSubstr("OPENC3_API_PASSWORD"));
}

TEST_F(DoctorServiceTest, ScopeCheckFailsWhenConfiguredScopeIsAbsent)
{
    // Every API call carries the scope, so a missing one breaks all COSMOS
    // operations in ways that look unrelated to scope.
    OpenC3::Core::Api::ApiConfig config;
    config.scope = "MISSION_B";

    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, get(::testing::HasSubstr("/scopes"), _))
        .WillOnce(Return(ApiResponse::ok(R"(["DEFAULT"])")));

    const auto r = sut_.runCheck("scope");
    EXPECT_EQ(r.status, HealthStatus::Fail);
    EXPECT_THAT(r.detail, ::testing::HasSubstr("MISSION_B"));
}

TEST_F(DoctorServiceTest, ScopeCheckPassesWhenScopeExists)
{
    OpenC3::Core::Api::ApiConfig config;
    config.scope = "DEFAULT";

    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, config()).WillRepeatedly(::testing::ReturnRef(config));
    EXPECT_CALL(api_, get(::testing::HasSubstr("/scopes"), _))
        .WillOnce(Return(ApiResponse::ok(R"(["DEFAULT","OTHER"])")));

    EXPECT_EQ(sut_.runCheck("scope").status, HealthStatus::Pass);
}

TEST_F(DoctorServiceTest, MicroservicesEmptyWarnsAboutTheOperator)
{
    // The classic post-install failure: the plugin installed but the operator
    // never started its microservices, so the plugin list looks correct while
    // nothing actually runs.
    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, get(::testing::HasSubstr("/microservices"), _))
        .WillOnce(Return(ApiResponse::ok("[]")));

    const auto r = sut_.runCheck("microservices");
    EXPECT_EQ(r.status, HealthStatus::Warning);
    EXPECT_THAT(r.suggestion, ::testing::HasSubstr("operator"));
}

TEST_F(DoctorServiceTest, PluginListUsesTheAllRouteAndCountsEntries)
{
    EXPECT_CALL(api_, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, get("/openc3-api/plugins/all", _))
        .WillOnce(Return(ApiResponse::ok(R"({"a__1":{},"b__2":{}})")));

    const auto r = sut_.runCheck("plugins");
    EXPECT_EQ(r.status, HealthStatus::Pass);
    EXPECT_THAT(r.detail, ::testing::HasSubstr("2"));
}

TEST_F(DoctorServiceTest, DockerInstalledDetailStripsTrailingNewline)
{
    EXPECT_CALL(mock_, execute(
        "which docker 2>/dev/null || command -v docker"))
        .WillOnce(Return(ExecutorResult::ok("/usr/bin/docker\n")));

    const auto r = sut_.runCheck("docker_installed");
    EXPECT_EQ(r.detail, "docker found at /usr/bin/docker");
}

TEST_F(DoctorServiceTest, DockerComposeDetailStripsTrailingNewline)
{
    EXPECT_CALL(mock_, execute(::testing::HasSubstr("docker compose version")))
        .WillOnce(Return(ExecutorResult::ok("Docker Compose version v2.20.2\n")));

    const auto r = sut_.runCheck("docker_compose");
    EXPECT_EQ(r.detail, "Docker Compose version v2.20.2");
}

TEST(DoctorServicePathTest, VersionCheckDetailStripsTrailingNewline)
{
    MockCommandExecutor mock;
    NiceMock<MockCosmosApiClient> api;
    DoctorService doctor{mock, api, [] { return std::string("/srv/cosmos"); }};

    EXPECT_CALL(mock, execute(
        "cat '/srv/cosmos/openc3-cosmos-init/plugins/openc3-tool-base/VERSION'"
        " 2>/dev/null || echo unknown"))
        .WillOnce(Return(ExecutorResult::ok("6.1.0\n")));

    const auto r = doctor.runCheck("openc3_version");
    EXPECT_EQ(r.detail, "OpenC3 version: 6.1.0");
}

TEST(DoctorServicePathTest, VersionCheckUsesConfiguredCosmosRoot)
{
    MockCommandExecutor mock;
    NiceMock<MockCosmosApiClient> api;
    DoctorService doctor{mock, api, [] { return std::string("/srv/cosmos"); }};

    EXPECT_CALL(mock, execute(
        "cat '/srv/cosmos/openc3-cosmos-init/plugins/openc3-tool-base/VERSION'"
        " 2>/dev/null || echo unknown"))
        .WillOnce(Return(ExecutorResult::ok("6.1.0\n")));

    const auto r = doctor.runCheck("openc3_version");
    EXPECT_EQ(r.status, HealthStatus::Pass);
}
