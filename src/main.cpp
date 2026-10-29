#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>

enum class EngagementState {
    Requested,
    Active,
    NeedsAttention,
    Completed,
    Cancelled
};
enum class Code {
    Success,
    InvalidInput,
    AlreadyExists,
    NotFound,
    InvalidTrack,
    TrackBusy,
    InvalidState
};

struct Result {
    Code code;
    std::string message;
};

struct Engagement {
    int id;
    int trackId;
    EngagementState state;
    std::optional<int> resourceId;
    std::string reason;
};

struct Resource {
    bool available;
    std::optional<int> ownerId;
};

// these functions handle state changes
bool start(Engagement& engagement);
bool complete(Engagement& engagement);
bool cancel(Engagement& engagement);

class EngagementManager {
public:
    Result registerTrack(int id, bool valid);
    Result updateTrack(int id, bool valid);
    Result registerResource(int id, bool available);
    Result updateResource(int id, bool available);
    Result start(int engagementId, int trackId);
    Result complete(int engagementId);
    Result cancel(int engagementId);
    std::optional<Engagement> getEngagement(int id) const;
    std::optional<Resource> getResource(int id) const;

private:
    std::map<int, bool> tracks;
    std::map<int, Resource> resources;
    std::map<int, Engagement> engagements;
    void releaseResource(Engagement& engagement);
    void allocateWaiting();
};


namespace {
Result result(Code code, const std::string& message) {
    return {code, message};
}

bool isTerminal(EngagementState state) {
    return state == EngagementState::Completed || state == EngagementState::Cancelled;
}
}

bool start(Engagement& engagement) {
    if (engagement.state != EngagementState::Requested) {
        return false;
    }
    engagement.state = EngagementState::Active;
    return true;
}

bool complete(Engagement& engagement) {
    if (engagement.state != EngagementState::Active) {
        return false;
    }
    engagement.state = EngagementState::Completed;
    return true;
}

bool cancel(Engagement& engagement) {
    if (engagement.state != EngagementState::Requested &&
        engagement.state != EngagementState::Active &&
        engagement.state != EngagementState::NeedsAttention) {
        return false;
    }
    engagement.state = EngagementState::Cancelled;
    return true;
}

Result EngagementManager::registerTrack(int id, bool valid) {
    if (id <= 0) {
        return result(Code::InvalidInput, "Track ID must be positive");
    }
    if (tracks.count(id) != 0) {
        return result(Code::AlreadyExists, "Track already exists");
    }
    tracks.emplace(id, valid);
    return result(Code::Success, "Track registered");
}

Result EngagementManager::updateTrack(int id, bool valid) {
    std::map<int, bool>::iterator trackIterator = tracks.find(id);
    if (trackIterator == tracks.end()) {
        return result(Code::NotFound, "Track not found");
    }
    if (trackIterator->second == valid) {
        return result(Code::Success, "Track unchanged");
    }
    trackIterator->second = valid;
    // invalid tracks need operator attention
    if (!valid) {
        for (std::pair<const int, Engagement>& engagementEntry : engagements) {
            Engagement& engagement = engagementEntry.second;
            if (engagement.trackId == id && !isTerminal(engagement.state)) {
                releaseResource(engagement);
                engagement.state = EngagementState::NeedsAttention;
                engagement.reason = "Track is invalid";
            }
        }
        allocateWaiting();
    }
    return result(Code::Success, "Track updated");
}

Result EngagementManager::registerResource(int id, bool available) {
    if (id <= 0) {
        return result(Code::InvalidInput, "Resource ID must be positive");
    }
    if (resources.count(id) != 0) {
        return result(Code::AlreadyExists, "Resource already exists");
    }
    resources.emplace(id, Resource{available, std::nullopt});
    allocateWaiting();
    return result(Code::Success, "Resource registered");
}

Result EngagementManager::updateResource(int id, bool available) {
    std::map<int, Resource>::iterator resourceIterator = resources.find(id);
    if (resourceIterator == resources.end()) {
        return result(Code::NotFound, "Resource not found");
    }
    if (resourceIterator->second.available == available) {
        return result(Code::Success, "Resource unchanged");
    }
    resourceIterator->second.available = available;
    // release the resource when it becomes unavailable
    if (!available && resourceIterator->second.ownerId) {
        Engagement& engagement = engagements.at(*resourceIterator->second.ownerId);
        releaseResource(engagement);
        engagement.state = EngagementState::NeedsAttention;
        engagement.reason = "Resource is unavailable";
    }
    allocateWaiting();
    return result(Code::Success, "Resource updated");
}

Result EngagementManager::start(int engagementId, int trackId) {
    if (engagementId <= 0 || trackId <= 0) {
        return result(Code::InvalidInput, "IDs must be positive");
    }
    if (engagements.count(engagementId) != 0) {
        return result(Code::AlreadyExists, "Engagement already exists");
    }
    std::map<int, bool>::iterator trackIterator = tracks.find(trackId);
    if (trackIterator == tracks.end()) {
        return result(Code::NotFound, "Track not found");
    }
    if (!trackIterator->second) {
        return result(Code::InvalidTrack, "Track is invalid");
    }
    // allow only one open engagement per track
    for (const std::pair<const int, Engagement>& engagementEntry : engagements) {
        if (engagementEntry.second.trackId == trackId && !isTerminal(engagementEntry.second.state)) {
            return result(Code::TrackBusy, "Track already has an open engagement");
        }
    }
    Engagement engagement{engagementId, trackId, EngagementState::Requested, std::nullopt, ""};
    engagements.emplace(engagementId, engagement);
    allocateWaiting();
    return result(Code::Success, "Engagement created");
}

Result EngagementManager::complete(int id) {
    std::map<int, Engagement>::iterator engagementIterator = engagements.find(id);
    if (engagementIterator == engagements.end()) {
        return result(Code::NotFound, "Engagement not found");
    }
    if (engagementIterator->second.state != EngagementState::Active) {
        return result(Code::InvalidState, "Only active engagements can complete");
    }
    releaseResource(engagementIterator->second);
    ::complete(engagementIterator->second);
    allocateWaiting();
    return result(Code::Success, "Engagement completed");
}

Result EngagementManager::cancel(int id) {
    std::map<int, Engagement>::iterator engagementIterator = engagements.find(id);
    if (engagementIterator == engagements.end()) {
        return result(Code::NotFound, "Engagement not found");
    }
    if (isTerminal(engagementIterator->second.state)) {
        return result(Code::InvalidState, "Engagement is already terminal");
    }
    releaseResource(engagementIterator->second);
    ::cancel(engagementIterator->second);
    allocateWaiting();
    return result(Code::Success, "Engagement cancelled");
}

std::optional<Engagement> EngagementManager::getEngagement(int id) const {
    std::map<int, Engagement>::const_iterator engagementIterator = engagements.find(id);
    if (engagementIterator == engagements.end()) {
        return std::nullopt;
    }
    return engagementIterator->second;
}

std::optional<Resource> EngagementManager::getResource(int id) const {
    std::map<int, Resource>::const_iterator resourceIterator = resources.find(id);
    if (resourceIterator == resources.end()) {
        return std::nullopt;
    }
    return resourceIterator->second;
}

// clear both sides of the resource assignment
void EngagementManager::releaseResource(Engagement& engagement) {
    if (engagement.resourceId) {
        resources.at(*engagement.resourceId).ownerId.reset();
        engagement.resourceId.reset();
    }
}

// maps assign resources in ascending ID order
void EngagementManager::allocateWaiting() {
    for (std::pair<const int, Engagement>& engagementEntry : engagements) {
        Engagement& engagement = engagementEntry.second;
        if (engagement.state != EngagementState::Requested) {
            continue;
        }
        for (std::pair<const int, Resource>& resourceEntry : resources) {
            Resource& resource = resourceEntry.second;
            if (resource.available && !resource.ownerId) {
                resource.ownerId = engagement.id;
                engagement.resourceId = resourceEntry.first;
                ::start(engagement);
                break;
            }
        }
    }
}

// the tests provide their own main function
#ifndef ENGAGEMENT_TEST_BUILD
int main() {
    EngagementManager manager;
    manager.registerTrack(500, true);
    manager.registerResource(1, true);
    manager.start(100, 500);
    std::optional<Engagement> engagement = manager.getEngagement(100);
    std::cout << "Engagement " << engagement->id << " started with resource " << *engagement->resourceId << '\n';
    manager.updateResource(1, false);
    engagement = manager.getEngagement(100);
    std::cout << "Engagement " << engagement->id << " needs attention: " << engagement->reason << '\n';
    manager.cancel(100);
    std::cout << "Engagement " << engagement->id << " cancelled by operator\n";
}
#endif
