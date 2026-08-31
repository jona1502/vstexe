#include <inputrack/EntitlementPolicy.h>
#include <inputrack/FeatureAccess.h>

#include <iostream>

namespace {
bool isPermanentOnly(const inputrack::EntitlementResult& result)
{
    return result.permanent && !result.trial && !result.trialAvailable
        && result.trialDaysRemaining == 0 && result.hasProAccess();
}
}

int main()
{
    using inputrack::ProductFeature;
    using inputrack::StorePurchaseOutcome;

    const inputrack::EntitlementResult free{false, false, true, 0, {}};
    const inputrack::EntitlementResult trial{false, true, false, 8, {}};
    const inputrack::EntitlementResult expired{false, false, false, 0, {}};

    const auto purchasedFromFree = inputrack::resolveStorePurchase(
        free, StorePurchaseOutcome::purchased);
    if (!isPermanentOnly(purchasedFromFree)
        || purchasedFromFree.message != "InputRack Pro is ready.") return 1;

    const auto purchasedFromTrial = inputrack::resolveStorePurchase(
        trial, StorePurchaseOutcome::purchased);
    if (!isPermanentOnly(purchasedFromTrial)) return 2;

    const auto cancelled = inputrack::resolveStorePurchase(
        trial, StorePurchaseOutcome::cancelled);
    if (!cancelled.trial || cancelled.permanent || cancelled.trialDaysRemaining != 8
        || cancelled.message != "The purchase was cancelled.") return 3;

    const auto failed = inputrack::resolveStorePurchase(
        free, StorePurchaseOutcome::failed);
    if (failed.hasProAccess() || !failed.trialAvailable
        || failed.message != "The Microsoft Store could not complete the request.") return 4;

    const auto restored = inputrack::resolveStoreLicense(expired, true);
    if (!isPermanentOnly(restored)
        || restored.message != "InputRack Pro purchase restored.") return 5;

    const auto activeTrialWithoutPurchase = inputrack::resolveStoreLicense(trial, false);
    if (!activeTrialWithoutPurchase.trial || activeTrialWithoutPurchase.permanent
        || activeTrialWithoutPurchase.trialDaysRemaining != 8
        || activeTrialWithoutPurchase.message.isEmpty()) return 6;

    const auto freeWithoutPurchase = inputrack::resolveStoreLicense(free, false);
    if (freeWithoutPurchase.hasProAccess() || !freeWithoutPurchase.trialAvailable
        || freeWithoutPurchase.message.isNotEmpty()) return 7;

    for (const auto proState : {purchasedFromFree, purchasedFromTrial, restored})
        if (!inputrack::hasFeatureAccess(ProductFeature::workflowProfiles, proState)
            || !inputrack::hasFeatureAccess(ProductFeature::automaticProfiles, proState)
            || !inputrack::hasFeatureAccess(ProductFeature::globalHotkeys, proState)) return 8;

    std::cout << "Entitlement policy tests passed\n";
    return 0;
}
