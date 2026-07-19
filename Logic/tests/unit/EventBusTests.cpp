#include <catch2/catch.hpp>

#include "events/EventBus.hpp"

using namespace kungfu;

namespace {
struct DummyEvent {
    int value;
};
}  // namespace

TEST_CASE("EventBus delivers a published event to a subscribed handler", "[events]") {
    EventBus<DummyEvent> bus;
    int received = -1;
    bus.subscribe([&](const DummyEvent& e) { received = e.value; });

    bus.publish({42});

    CHECK(received == 42);
}

TEST_CASE("EventBus delivers one publish to every subscriber", "[events]") {
    EventBus<DummyEvent> bus;
    int a = 0;
    int b = 0;
    bus.subscribe([&](const DummyEvent& e) { a = e.value; });
    bus.subscribe([&](const DummyEvent& e) { b = e.value * 2; });

    bus.publish({5});

    CHECK(a == 5);
    CHECK(b == 10);
}

TEST_CASE("EventBus stops calling a handler after unsubscribe", "[events]") {
    EventBus<DummyEvent> bus;
    int callCount = 0;
    const size_t id = bus.subscribe([&](const DummyEvent&) { ++callCount; });

    bus.publish({1});
    bus.unsubscribe(id);
    bus.publish({1});

    CHECK(callCount == 1);
}

TEST_CASE("EventBus with no subscribers is a safe no-op", "[events]") {
    EventBus<DummyEvent> bus;
    CHECK_NOTHROW(bus.publish({1}));
}
