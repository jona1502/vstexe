#pragma once
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>

namespace inputrack {
struct EntitlementResult {
    bool permanent{};
    bool trial{};
    bool trialAvailable{};
    int trialDaysRemaining{};
    juce::String message;
    bool hasProAccess() const noexcept { return permanent || trial; }
};

class EntitlementService {
public:
    using Callback = std::function<void(EntitlementResult)>;
    virtual ~EntitlementService() = default;
    virtual EntitlementResult state() const noexcept = 0;
    virtual EntitlementResult refreshLocalState() noexcept = 0;
    virtual bool isBusy() const noexcept = 0;
    virtual void refresh(void* ownerWindow, Callback) = 0;
    virtual void purchase(void* ownerWindow, Callback) = 0;
    virtual void startTrial(void* ownerWindow, Callback) = 0;

    static std::unique_ptr<EntitlementService> create();
};
}
