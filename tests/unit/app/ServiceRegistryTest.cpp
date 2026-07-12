#include "app/ServiceRegistry.h"
#include <gtest/gtest.h>

using OpenC3::App::ServiceRegistry;

namespace {

struct IGreeter {
    virtual ~IGreeter()               = default;
    virtual std::string greet() const = 0;
};

struct FriendlyGreeter final : IGreeter {
    std::string greet() const override { return "hello"; }
};

struct IOther {
    virtual ~IOther() = default;
};

struct OtherImpl final : IOther {};

} // namespace

TEST(ServiceRegistryTest, ResolveReturnsRegisteredInstance)
{
    ServiceRegistry registry;
    registry.registerInstance<IGreeter>(std::make_shared<FriendlyGreeter>());

    const auto greeter = registry.resolve<IGreeter>();
    ASSERT_NE(greeter, nullptr);
    EXPECT_EQ(greeter->greet(), "hello");
}

TEST(ServiceRegistryTest, ResolveReturnsSameSharedInstanceEachTime)
{
    ServiceRegistry registry;
    auto instance = std::make_shared<FriendlyGreeter>();
    registry.registerInstance<IGreeter>(instance);

    const auto first  = registry.resolve<IGreeter>();
    const auto second = registry.resolve<IGreeter>();
    EXPECT_EQ(first.get(), instance.get());
    EXPECT_EQ(first.get(), second.get());
}

TEST(ServiceRegistryTest, ResolveUnregisteredTypeThrows)
{
    ServiceRegistry registry;
    // (void) — resolve() is [[nodiscard]]; MSVC /W4 /WX turns the discarded
    // return inside EXPECT_THROW into a hard error (C4834).
    EXPECT_THROW((void)registry.resolve<IGreeter>(), std::runtime_error);
}

// Every service is meant to be registered exactly once (singleton lifetime,
// per the class comment) - registering the same interface twice is a wiring
// bug, not a valid "replace" operation, so it must fail loudly instead of
// silently overwriting the first instance.
TEST(ServiceRegistryTest, DoubleRegisteringSameInterfaceThrows)
{
    ServiceRegistry registry;
    registry.registerInstance<IGreeter>(std::make_shared<FriendlyGreeter>());
    EXPECT_THROW(
        registry.registerInstance<IGreeter>(std::make_shared<FriendlyGreeter>()),
        std::runtime_error);
}

TEST(ServiceRegistryTest, DifferentInterfacesDoNotCollide)
{
    ServiceRegistry registry;
    registry.registerInstance<IGreeter>(std::make_shared<FriendlyGreeter>());
    registry.registerInstance<IOther>(std::make_shared<OtherImpl>());

    EXPECT_NE(registry.resolve<IGreeter>(), nullptr);
    EXPECT_NE(registry.resolve<IOther>(), nullptr);
}
