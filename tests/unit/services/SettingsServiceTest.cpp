#include "services/settings/SettingsService.h"
#include "models/ConnectionProfile.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace OpenC3::Services;
using namespace OpenC3::Models;
namespace fs = std::filesystem;

class SettingsServiceTest : public ::testing::Test {
protected:
    fs::path   tempFile_{fs::temp_directory_path() / "opencosmos_test_settings.json"};
    SettingsService sut_{tempFile_.string()};

    void TearDown() override {
        fs::remove(tempFile_);
    }

    ConnectionProfile makeProfile(const std::string& id,
                                   const std::string& name) const {
        ConnectionProfile p;
        p.id   = id;
        p.name = name;
        p.mode = ConnectionMode::WSL;
        return p;
    }
};

TEST_F(SettingsServiceTest, InitiallyHasNoProfiles)
{
    EXPECT_TRUE(sut_.profiles().empty());
}

TEST_F(SettingsServiceTest, SavedProfileIsRetrievable)
{
    sut_.saveProfile(makeProfile("id1", "Dev"));
    const auto profiles = sut_.profiles();
    ASSERT_EQ(profiles.size(), 1u);
    EXPECT_EQ(profiles[0].name, "Dev");
}

TEST_F(SettingsServiceTest, ProfileByIdReturnsCorrectProfile)
{
    sut_.saveProfile(makeProfile("aaa", "A"));
    sut_.saveProfile(makeProfile("bbb", "B"));
    const auto p = sut_.profileById("bbb");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->name, "B");
}

TEST_F(SettingsServiceTest, ProfileByIdReturnsNulloptForMissing)
{
    const auto p = sut_.profileById("nonexistent");
    EXPECT_FALSE(p.has_value());
}

TEST_F(SettingsServiceTest, DeleteProfileRemovesIt)
{
    sut_.saveProfile(makeProfile("id1", "Dev"));
    sut_.deleteProfile("id1");
    EXPECT_TRUE(sut_.profiles().empty());
}

TEST_F(SettingsServiceTest, SetDefaultProfileMarksCorrectProfile)
{
    sut_.saveProfile(makeProfile("id1", "A"));
    sut_.saveProfile(makeProfile("id2", "B"));
    sut_.setDefaultProfile("id2");

    const auto def = sut_.defaultProfile();
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->id, "id2");
}

TEST_F(SettingsServiceTest, StringSettingRoundTrips)
{
    sut_.setString("theme", "dark");
    EXPECT_EQ(sut_.getString("theme", "light"), "dark");
}

TEST_F(SettingsServiceTest, IntSettingRoundTrips)
{
    sut_.setInt("refreshIntervalMs", 5000);
    EXPECT_EQ(sut_.getInt("refreshIntervalMs", 0), 5000);
}

TEST_F(SettingsServiceTest, BoolSettingRoundTrips)
{
    sut_.setBool("autoConnect", true);
    EXPECT_TRUE(sut_.getBool("autoConnect", false));
}

TEST_F(SettingsServiceTest, PersistenceAcrossReload)
{
    sut_.saveProfile(makeProfile("id1", "Persisted"));
    sut_.setString("key", "value");

    SettingsService loaded{tempFile_.string()};
    loaded.load();

    const auto profiles = loaded.profiles();
    ASSERT_EQ(profiles.size(), 1u);
    EXPECT_EQ(profiles[0].name, "Persisted");
    EXPECT_EQ(loaded.getString("key", ""), "value");
}

TEST_F(SettingsServiceTest, SshSecretsAreProtectedOnDiskAndRoundTrip)
{
    auto profile = makeProfile("ssh1", "SSH");
    profile.mode = ConnectionMode::SSH;
    profile.password = "secret-password";
    profile.passphrase = "secret-passphrase";

    sut_.saveProfile(profile);

    std::ifstream f(tempFile_);
    const std::string saved((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    EXPECT_EQ(saved.find("secret-password"), std::string::npos);
    EXPECT_EQ(saved.find("secret-passphrase"), std::string::npos);
    EXPECT_NE(saved.find("enc:v1:"), std::string::npos);

    SettingsService loaded{tempFile_.string()};
    loaded.load();
    const auto roundTrip = loaded.profileById("ssh1");
    ASSERT_TRUE(roundTrip.has_value());
    EXPECT_EQ(roundTrip->password, "secret-password");
    EXPECT_EQ(roundTrip->passphrase, "secret-passphrase");
}

TEST_F(SettingsServiceTest, LegacyPlaintextSshSecretsStillLoad)
{
    {
        std::ofstream f(tempFile_);
        f << R"({
  "profiles": [
    {
      "id": "legacy",
      "name": "Legacy",
      "mode": 1,
      "isDefault": false,
      "cosmosRootPath": "/cosmos",
      "wslDistribution": "Ubuntu",
      "host": "example.test",
      "port": 22,
      "username": "operator",
      "authMethod": 0,
      "password": "plain-password",
      "privateKeyPath": "",
      "publicKeyPath": "",
      "passphrase": "plain-passphrase",
      "connectTimeoutMs": 10000,
      "commandTimeoutMs": 30000
    }
  ],
  "settings": {}
})";
    }

    SettingsService loaded{tempFile_.string()};
    loaded.load();
    const auto profile = loaded.profileById("legacy");
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->password, "plain-password");
    EXPECT_EQ(profile->passphrase, "plain-passphrase");
}
