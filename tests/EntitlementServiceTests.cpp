#include <inputrack/EntitlementService.h>
#include <inputrack/FeatureAccess.h>
#include <inputrack/TrialPolicy.h>

#include <cstdint>

int main()
{
    constexpr std::uint64_t day = 24ULL * 60ULL * 60ULL;
    constexpr std::uint64_t started = 1'800'000'000ULL;
    const auto unused = inputrack::evaluateTrialState(std::nullopt, started);
    if (unused.active || !unused.available || unused.daysRemaining != 0) return 4;
    const auto active = inputrack::evaluateTrialState(started, started + 2 * day);
    if (!active.active || active.available || active.daysRemaining != 12) return 5;
    const auto finalDay = inputrack::evaluateTrialState(started, started + 13 * day + 1);
    if (!finalDay.active || finalDay.daysRemaining != 1) return 6;
    const auto expired = inputrack::evaluateTrialState(started, started + 14 * day);
    if (expired.active || expired.available || expired.daysRemaining != 0) return 7;
    const auto clockRollback = inputrack::evaluateTrialState(started, started - day);
    if (!clockRollback.active || clockRollback.daysRemaining != 14) return 8;

    inputrack::EntitlementResult free;
    inputrack::EntitlementResult trial{false, true, false, 14, {}};
    inputrack::EntitlementResult permanent{true, false, false, 0, {}};
    if (free.hasProAccess() || !trial.hasProAccess() || !permanent.hasProAccess()) return 3;
    using inputrack::ProductFeature;
    if (!inputrack::hasFeatureAccess(ProductFeature::rackPresets, free)
        || !inputrack::hasFeatureAccess(ProductFeature::windowsStartup, free)
        || inputrack::hasFeatureAccess(ProductFeature::workflowProfiles, free)
        || inputrack::hasFeatureAccess(ProductFeature::automaticProfiles, free)
        || inputrack::hasFeatureAccess(ProductFeature::globalHotkeys, free)) return 9;
    for (const auto feature : {ProductFeature::workflowProfiles,
                               ProductFeature::automaticProfiles,
                               ProductFeature::globalHotkeys}) {
        if (!inputrack::hasFeatureAccess(feature, trial)
            || !inputrack::hasFeatureAccess(feature, permanent)) return 10;
    }

    const auto service = inputrack::EntitlementService::create();
    if (!service->state().hasProAccess() || service->state().trial
        || service->state().trialAvailable || service->isBusy()) return 1;
    auto called = false;
    service->refresh(nullptr, [&called](inputrack::EntitlementResult result) {
        called = result.permanent && result.hasProAccess() && result.message.isEmpty();
    });
    return called ? 0 : 2;
}
