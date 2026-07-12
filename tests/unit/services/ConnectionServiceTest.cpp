#include "services/connection/ConnectionService.h"
#include "services/settings/ISettingsService.h"

#include <gtest/gtest.h>

using namespace OpenC3::Services;

namespace {

// Minimal fake - only profileById() is exercised by the paths under test
// (an unknown profile id), which never touches a real executor/network.
class FakeSettingsService final : public ISettingsService {
public:
    std::vector<OpenC3::Models::ConnectionProfile> profiles() const override { return {}; }
    std::optional<OpenC3::Models::ConnectionProfile> profileById(const std::string&) const override
    {
        return std::nullopt;
    }
    std::optional<OpenC3::Models::ConnectionProfile> defaultProfile() const override
    {
        return std::nullopt;
    }
    void saveProfile(const OpenC3::Models::ConnectionProfile&) override {}
    void deleteProfile(const std::string&) override {}
    void setDefaultProfile(const std::string&) override {}
    std::string getString(const std::string&, const std::string& fallback) const override
    {
        return fallback;
    }
    int  getInt(const std::string&, int fallback) const override { return fallback; }
    bool getBool(const std::string&, bool fallback) const override { return fallback; }
    void setString(const std::string&, const std::string&) override {}
    void setInt(const std::string&, int) override {}
    void setBool(const std::string&, bool) override {}
    void load() override {}
    void save() override {}
};

} // namespace

TEST(ConnectionServiceTest, StartsDisconnectedWithNoActiveProfile)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    EXPECT_EQ(svc.state(), ConnectionState::Disconnected);
    EXPECT_EQ(svc.activeProfile(), nullptr);
    EXPECT_EQ(svc.cosmosRootPath(), "/cosmos");
    EXPECT_EQ(svc.executor(), nullptr);
}

TEST(ConnectionServiceTest, ConnectWithUnknownProfileIdFailsWithoutTouchingAnExecutor)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    EXPECT_FALSE(svc.connect("does-not-exist"));
    EXPECT_EQ(svc.state(), ConnectionState::Error);
    EXPECT_EQ(svc.activeProfile(), nullptr);
    EXPECT_EQ(svc.executor(), nullptr);
}

TEST(ConnectionServiceTest, DisconnectWhenNeverConnectedIsANoOp)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    EXPECT_NO_THROW(svc.disconnect());
    EXPECT_EQ(svc.state(), ConnectionState::Disconnected);
}

// setState() fires Connecting, then Error, for an unknown profile id - every
// registered callback must see both transitions with the profileId/errorMessage
// populated, since Application::registerServices() relies on Connecting firing
// to reset ExecutorProxy before connect() reassigns executor_ (see the comment
// there); a callback that only fired on the *last* state would break that.
TEST(ConnectionServiceTest, OnStateChangedFiresConnectingThenErrorForUnknownProfile)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    std::vector<ConnectionState> seenStates;
    std::string                  lastError;
    svc.onStateChanged([&](const ConnectionEvent& ev) {
        seenStates.push_back(ev.state);
        lastError = ev.errorMessage;
    });

    (void)svc.connect("missing-profile");

    ASSERT_EQ(seenStates.size(), 2u);
    EXPECT_EQ(seenStates[0], ConnectionState::Connecting);
    EXPECT_EQ(seenStates[1], ConnectionState::Error);
    EXPECT_NE(lastError.find("missing-profile"), std::string::npos);
}

TEST(ConnectionServiceTest, MultipleCallbacksAllReceiveEvents)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    int firstCount = 0, secondCount = 0;
    svc.onStateChanged([&](const ConnectionEvent&) { ++firstCount; });
    svc.onStateChanged([&](const ConnectionEvent&) { ++secondCount; });

    (void)svc.connect("missing-profile");

    EXPECT_EQ(firstCount, 2);
    EXPECT_EQ(secondCount, 2);
}

// ViewModels unsubscribe in their destructors; a removed callback must never
// fire again while the remaining subscribers keep receiving events.
TEST(ConnectionServiceTest, RemoveStateChangedStopsOnlyThatCallback)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    int removedCount = 0, keptCount = 0;
    const auto id = svc.onStateChanged([&](const ConnectionEvent&) { ++removedCount; });
    svc.onStateChanged([&](const ConnectionEvent&) { ++keptCount; });

    svc.removeStateChanged(id);
    (void)svc.connect("missing-profile");

    EXPECT_EQ(removedCount, 0);
    EXPECT_EQ(keptCount, 2);
}

TEST(ConnectionServiceTest, RemoveStateChangedWithUnknownIdIsANoOp)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    int count = 0;
    svc.onStateChanged([&](const ConnectionEvent&) { ++count; });

    EXPECT_NO_THROW(svc.removeStateChanged(9999));
    (void)svc.connect("missing-profile");

    EXPECT_EQ(count, 2);
}

// A callback may unsubscribe itself while events are being dispatched (the
// mutex is recursive); this must not invalidate the dispatch loop.
TEST(ConnectionServiceTest, CallbackCanRemoveItselfDuringDispatch)
{
    FakeSettingsService settings;
    ConnectionService   svc(settings);

    int selfCount = 0, otherCount = 0;
    IConnectionService::SubscriptionId selfId = 0;
    selfId = svc.onStateChanged([&](const ConnectionEvent&) {
        ++selfCount;
        svc.removeStateChanged(selfId);
    });
    svc.onStateChanged([&](const ConnectionEvent&) { ++otherCount; });

    // connect() fires Connecting then Error; the self-removing callback only
    // sees the first event, the other callback sees both.
    (void)svc.connect("missing-profile");

    EXPECT_EQ(selfCount, 1);
    EXPECT_EQ(otherCount, 2);
}
