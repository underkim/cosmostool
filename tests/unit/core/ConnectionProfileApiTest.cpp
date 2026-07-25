#include "models/ConnectionProfile.h"

#include <gtest/gtest.h>

using OpenC3::Models::ConnectionProfile;
using OpenC3::Models::ConnectionMode;

namespace {

TEST(ConnectionProfileApiTest, DefaultProfileHasNoApiEndpoint)
{
    // A profile carried over from a build without API support must not be
    // treated as API-capable, or every COSMOS call would fail authorization
    // with an empty password instead of cleanly falling back to shell-only.
    const ConnectionProfile profile;
    EXPECT_FALSE(profile.hasApiEndpoint());
    EXPECT_EQ(profile.apiScope, "DEFAULT");
}

TEST(ConnectionProfileApiTest, ApiPasswordAloneEnablesTheEndpoint)
{
    ConnectionProfile profile;
    profile.apiPassword = "secret";
    EXPECT_TRUE(profile.hasApiEndpoint());
}

TEST(ConnectionProfileApiTest, WslProfileDefaultsToLocalhostEndpoint)
{
    ConnectionProfile profile;
    profile.mode = ConnectionMode::WSL;
    // WSL forwards localhost to the distro, so the Windows-side toolkit can
    // reach traefik on the standard port without any extra configuration.
    EXPECT_EQ(profile.resolvedApiBaseUrl(), "http://localhost:2900");
}

TEST(ConnectionProfileApiTest, SshProfileDerivesEndpointFromHost)
{
    ConnectionProfile profile;
    profile.mode = ConnectionMode::SSH;
    profile.host = "cosmos.example.com";
    EXPECT_EQ(profile.resolvedApiBaseUrl(), "http://cosmos.example.com:2900");
}

TEST(ConnectionProfileApiTest, ExplicitBaseUrlWins)
{
    ConnectionProfile profile;
    profile.mode       = ConnectionMode::SSH;
    profile.host       = "cosmos.example.com";
    profile.apiBaseUrl = "https://gateway.internal:8443";
    EXPECT_EQ(profile.resolvedApiBaseUrl(), "https://gateway.internal:8443");
}

TEST(ConnectionProfileApiTest, ToApiConfigCarriesCredentialsAndScope)
{
    ConnectionProfile profile;
    profile.apiPassword         = "hunter2";
    profile.apiScope            = "OTHER";
    profile.apiRequestTimeoutMs = 45'000;
    profile.apiAllowInsecureTls = true;

    const auto config = profile.toApiConfig();
    EXPECT_EQ(config.password, "hunter2");
    EXPECT_EQ(config.scope, "OTHER");
    EXPECT_EQ(config.requestTimeoutMs, 45'000);
    EXPECT_TRUE(config.allowInsecureTls);
}

TEST(ConnectionProfileApiTest, EmptyScopeFallsBackToDefault)
{
    // COSMOS's authorize() raises "Scope is required", so an empty scope must
    // never reach the wire - a blanked-out settings field becomes DEFAULT.
    ConnectionProfile profile;
    profile.apiScope = "";
    EXPECT_EQ(profile.toApiConfig().scope, "DEFAULT");
}

} // namespace
