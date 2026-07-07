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

// publish() snapshots the handler list before invoking it (see the comment on
// EventBus::publish) specifically so a handler can subscribe a new handler
// for the same event type without corrupting the in-progress dispatch loop.
// That guarantee had no test coverage: a naive implementation that iterated
// handlers_ directly (instead of over a snapshot) would invalidate iterators
// or invoke the newly-added handler within the same publish() call.
TEST(EventBus, HandlerSubscribingDuringPublishDoesNotJoinCurrentDispatch)
{
    EventBus bus;
    int firstCallCount  = 0;
    int secondCallCount = 0;

    bus.subscribe<TestEvent>([&](const TestEvent&) {
        ++firstCallCount;
        bus.subscribe<TestEvent>([&](const TestEvent&) { ++secondCallCount; });
    });

    bus.publish(TestEvent{1});
    EXPECT_EQ(firstCallCount, 1);
    EXPECT_EQ(secondCallCount, 0); // added during dispatch - must not fire yet

    bus.publish(TestEvent{2});
    EXPECT_EQ(firstCallCount, 2);
    EXPECT_EQ(secondCallCount, 1); // now present for this second dispatch
}

// A handler re-entering publish() for the same event type must not deadlock
// (the snapshot is taken and the lock released before handlers run) and must
// not itself observe the handler that's still executing higher up the stack.
TEST(EventBus, HandlerPublishingSameEventTypeDoesNotDeadlock)
{
    EventBus bus;
    int outerCount = 0;
    int innerCount = 0;

    bus.subscribe<TestEvent>([&](const TestEvent& e) {
        ++outerCount;
        if (e.value == 1) {
            innerCount = outerCount;
            bus.publish(TestEvent{2});
        }
    });

    bus.publish(TestEvent{1});
    EXPECT_EQ(outerCount, 2);
    EXPECT_EQ(innerCount, 1);
}
