#include "core/events/EventBus.h"
#include <gtest/gtest.h>

using OpenC3::Core::Events::EventBus;

struct TestEvent {
    int value{0};
};

struct AnotherEvent {
    std::string name;
};

TEST(EventBus, SubscriberReceivesPublishedEvent)
{
    EventBus bus;
    int received = -1;

    bus.subscribe<TestEvent>([&](const TestEvent& e) {
        received = e.value;
    });

    bus.publish(TestEvent{42});
    EXPECT_EQ(received, 42);
}

TEST(EventBus, MultipleSubscribersAllReceive)
{
    EventBus bus;
    int a = 0, b = 0;

    bus.subscribe<TestEvent>([&](const TestEvent& e){ a = e.value; });
    bus.subscribe<TestEvent>([&](const TestEvent& e){ b = e.value; });

    bus.publish(TestEvent{7});
    EXPECT_EQ(a, 7);
    EXPECT_EQ(b, 7);
}

TEST(EventBus, DifferentEventTypesDoNotCross)
{
    EventBus bus;
    bool testFired    = false;
    bool anotherFired = false;

    bus.subscribe<TestEvent>   ([&](const TestEvent&)   { testFired    = true; });
    bus.subscribe<AnotherEvent>([&](const AnotherEvent&){ anotherFired = true; });

    bus.publish(TestEvent{1});
    EXPECT_TRUE(testFired);
    EXPECT_FALSE(anotherFired);
}

TEST(EventBus, ClearRemovesAllHandlers)
{
    EventBus bus;
    int count = 0;
    bus.subscribe<TestEvent>([&](const TestEvent&){ ++count; });
    bus.clear();
    bus.publish(TestEvent{1});
    EXPECT_EQ(count, 0);
}

TEST(EventBus, NoHandlerPublishDoesNotCrash)
{
    EventBus bus;
    EXPECT_NO_THROW(bus.publish(TestEvent{99}));
}
