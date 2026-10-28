
#include <iostream>

enum class EngagementState {
    Requested,
    Active,
    Completed,
    Cancelled
};

struct Engagement {
    int id;
    EngagementState state;
};

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
        engagement.state != EngagementState::Active) {
        return false;
    }

    engagement.state = EngagementState::Cancelled;
    return true;
}


int main() {
    Engagement engagement{
        100,
        EngagementState::Requested
    };

    std::cout << "Engagement ID: " << engagement.id << "\n";

    // First attempt
    bool started = start(engagement);

    if (started) {
        std::cout << "First attempt: started successfully\n";
    } else {
        std::cout << "First attempt: rejected\n";
    }

    if (engagement.state == EngagementState::Active) {
        std::cout << "Current state: Active\n";
    }

    // Second attempt
    started = start(engagement);

    if (started) {
        std::cout << "Second attempt: started successfully\n";
    } else {
        std::cout << "Second attempt: rejected\n";
    }

    // Try to complete the engagement
    bool completed = complete(engagement);

    if (completed) {
        std::cout << "Completion: successful\n";
    } else {
        std::cout << "Completion: rejected\n";
    }

    if (engagement.state == EngagementState::Completed) {
        std::cout << "Current state: Completed\n";
    }

    // Try to start the completed engagement
    started = start(engagement);

    if (started) {
        std::cout << "Restart attempt: successful\n";
    } else {
        std::cout << "Restart attempt: rejected\n";
    }


    // Create another engagement
    Engagement secondEngagement{
        101,
        EngagementState::Requested
    };

    std::cout << "\nEngagement ID: " << secondEngagement.id << "\n";

    // Cancel it before it starts
    bool cancelled = cancel(secondEngagement);

    if (cancelled) {
        std::cout << "Cancellation: successful\n";
    } else {
        std::cout << "Cancellation: rejected\n";
    }

    if (secondEngagement.state == EngagementState::Cancelled) {
        std::cout << "Current state: Cancelled\n";
    }

    // Try to start the cancelled engagement
    started = start(secondEngagement);

    if (started) {
        std::cout << "Start after cancellation: successful\n";
    } else {
        std::cout << "Start after cancellation: rejected\n";
    }

    return 0;
}
