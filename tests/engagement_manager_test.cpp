#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest.h"

#define ENGAGEMENT_TEST_BUILD
#include "../src/main.cpp"

Engagement requireEngagement(const EngagementManager& manager, int id) {
    auto engagement = manager.getEngagement(id);
    REQUIRE(engagement.has_value());
    return *engagement;
}

Resource requireResource(const EngagementManager& manager, int id) {
    auto resource = manager.getResource(id);
    REQUIRE(resource.has_value());
    return *resource;
}

// checks which state changes are allowed
TEST_CASE("Original state transitions") {
    Engagement engagement{100, 500, EngagementState::Requested, std::nullopt, ""};
    CHECK_FALSE(complete(engagement));
    CHECK(engagement.state == EngagementState::Requested);
    CHECK(start(engagement));
    CHECK_FALSE(start(engagement));
    CHECK(complete(engagement));
    CHECK_FALSE(start(engagement));
    CHECK_FALSE(cancel(engagement));
    Engagement second{101, 501, EngagementState::Requested, std::nullopt, ""};
    CHECK(cancel(second));
    CHECK_FALSE(start(second));
    Engagement third{102, 502, EngagementState::Requested, std::nullopt, ""};
    REQUIRE(start(third));
    CHECK(cancel(third));
}

// checks resource assignment and waiting
TEST_CASE("Resource allocation and waiting") {
    EngagementManager manager;
    REQUIRE(manager.registerTrack(500, true).code == Code::Success);
    REQUIRE(manager.registerTrack(501, true).code == Code::Success);
    REQUIRE(manager.registerResource(1, true).code == Code::Success);
    REQUIRE(manager.start(100, 500).code == Code::Success);
    CHECK(requireEngagement(manager, 100).state == EngagementState::Active);
    CHECK(requireEngagement(manager, 100).resourceId == std::optional<int>(1));
    CHECK(requireResource(manager, 1).ownerId == std::optional<int>(100));
    REQUIRE(manager.start(101, 501).code == Code::Success);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Requested);
    REQUIRE(manager.complete(100).code == Code::Success);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Active);
    CHECK(requireResource(manager, 1).ownerId == std::optional<int>(101));
    CHECK(manager.complete(100).code == Code::InvalidState);
    REQUIRE(manager.cancel(101).code == Code::Success);
    CHECK(!requireResource(manager, 1).ownerId);
    CHECK(!requireEngagement(manager, 101).resourceId);
    CHECK(manager.cancel(101).code == Code::InvalidState);
}

// checks invalid inputs and state changes
TEST_CASE("Invalid inputs and transitions") {
    EngagementManager manager;
    CHECK(manager.registerTrack(0, true).code == Code::InvalidInput);
    REQUIRE(manager.registerTrack(500, false).code == Code::Success);
    CHECK(manager.registerTrack(500, true).code == Code::AlreadyExists);
    CHECK(manager.start(100, 500).code == Code::InvalidTrack);
    CHECK(!manager.getEngagement(100));
    CHECK(manager.start(100, 999).code == Code::NotFound);
    REQUIRE(manager.updateTrack(500, true).code == Code::Success);
    REQUIRE(manager.start(100, 500).code == Code::Success);
    CHECK(manager.start(101, 500).code == Code::TrackBusy);
    CHECK(manager.start(100, 500).code == Code::AlreadyExists);
    CHECK(manager.complete(100).code == Code::InvalidState);
    CHECK(requireEngagement(manager, 100).state == EngagementState::Requested);
}

// checks resource and track failures
TEST_CASE("Failures and recovery boundary") {
    EngagementManager manager;
    REQUIRE(manager.registerTrack(500, true).code == Code::Success);
    REQUIRE(manager.registerTrack(501, true).code == Code::Success);
    REQUIRE(manager.registerResource(1, true).code == Code::Success);
    REQUIRE(manager.start(100, 500).code == Code::Success);
    REQUIRE(manager.start(101, 501).code == Code::Success);
    REQUIRE(manager.updateResource(1, false).code == Code::Success);
    auto first = requireEngagement(manager, 100);
    CHECK(first.state == EngagementState::NeedsAttention);
    CHECK_FALSE(first.resourceId.has_value());
    CHECK(!first.reason.empty());
    CHECK(!requireResource(manager, 1).ownerId);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Requested);
    CHECK(manager.complete(100).code == Code::InvalidState);
    REQUIRE(manager.updateResource(1, false).code == Code::Success);
    REQUIRE(manager.updateResource(1, true).code == Code::Success);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Active);
    CHECK(requireEngagement(manager, 100).state == EngagementState::NeedsAttention);
    REQUIRE(manager.updateTrack(501, false).code == Code::Success);
    CHECK(requireEngagement(manager, 101).state == EngagementState::NeedsAttention);
    CHECK(!requireResource(manager, 1).ownerId);
    CHECK(manager.complete(101).code == Code::InvalidState);
    REQUIRE(manager.updateTrack(500, false).code == Code::Success);
    REQUIRE(manager.updateTrack(500, true).code == Code::Success);
    CHECK(manager.start(102, 500).code == Code::TrackBusy);
    REQUIRE(manager.cancel(100).code == Code::Success);
    REQUIRE(manager.start(102, 500).code == Code::Success);
}

// checks assignment order and repeated updates
TEST_CASE("Allocation and repeated updates") {
    EngagementManager manager;
    REQUIRE(manager.registerTrack(500, true).code == Code::Success);
    REQUIRE(manager.registerTrack(501, true).code == Code::Success);
    REQUIRE(manager.start(101, 501).code == Code::Success);
    REQUIRE(manager.start(100, 500).code == Code::Success);
    REQUIRE(manager.registerResource(1, true).code == Code::Success);
    CHECK(requireEngagement(manager, 100).state == EngagementState::Active);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Requested);
    REQUIRE(manager.updateResource(1, true).code == Code::Success);
    CHECK(requireResource(manager, 1).ownerId == std::optional<int>(100));
    REQUIRE(manager.updateTrack(500, true).code == Code::Success);
    CHECK(requireEngagement(manager, 100).state == EngagementState::Active);
    REQUIRE(manager.updateTrack(500, false).code == Code::Success);
    CHECK(requireEngagement(manager, 100).state == EngagementState::NeedsAttention);
    CHECK(requireResource(manager, 1).ownerId == std::optional<int>(101));
    REQUIRE(manager.registerTrack(502, true).code == Code::Success);
    REQUIRE(manager.start(102, 502).code == Code::Success);
    CHECK(requireEngagement(manager, 102).state == EngagementState::Requested);
    REQUIRE(manager.cancel(102).code == Code::Success);
    REQUIRE(manager.registerResource(2, true).code == Code::Success);
    CHECK(requireEngagement(manager, 102).state == EngagementState::Cancelled);
}

// checks errors and unaffected engagements
TEST_CASE("Errors and isolation") {
    EngagementManager manager;
    CHECK(manager.complete(100).code == Code::NotFound);
    CHECK(manager.cancel(100).code == Code::NotFound);
    CHECK(manager.updateTrack(500, true).code == Code::NotFound);
    CHECK(manager.updateResource(1, true).code == Code::NotFound);

    REQUIRE(manager.registerTrack(500, true).code == Code::Success);
    REQUIRE(manager.registerTrack(501, true).code == Code::Success);
    REQUIRE(manager.registerResource(1, true).code == Code::Success);
    REQUIRE(manager.registerResource(2, true).code == Code::Success);
    REQUIRE(manager.start(100, 500).code == Code::Success);
    REQUIRE(manager.start(101, 501).code == Code::Success);
    CHECK(manager.registerResource(2, false).code == Code::AlreadyExists);
    CHECK(requireResource(manager, 2).available);
    CHECK(requireResource(manager, 2).ownerId == std::optional<int>(101));
    REQUIRE(manager.updateResource(1, false).code == Code::Success);
    CHECK(!requireEngagement(manager, 100).resourceId);
    CHECK(!requireResource(manager, 1).ownerId);
    CHECK(requireEngagement(manager, 101).state == EngagementState::Active);
    CHECK(requireEngagement(manager, 101).resourceId == std::optional<int>(2));
    CHECK(requireResource(manager, 2).ownerId == std::optional<int>(101));
}


