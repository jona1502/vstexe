#include <inputrack/EntitlementPolicy.h>

namespace inputrack {
namespace {
void grantPermanentAccess(EntitlementResult& result)
{
    result.permanent = true;
    result.trial = false;
    result.trialAvailable = false;
    result.trialDaysRemaining = 0;
}
}

EntitlementResult resolveStorePurchase(EntitlementResult current,
                                       StorePurchaseOutcome outcome)
{
    switch (outcome) {
        case StorePurchaseOutcome::purchased:
            grantPermanentAccess(current);
            current.message = "InputRack Pro is ready.";
            break;
        case StorePurchaseOutcome::cancelled:
            current.message = "The purchase was cancelled.";
            break;
        case StorePurchaseOutcome::failed:
            current.message = "The Microsoft Store could not complete the request.";
            break;
    }
    return current;
}

EntitlementResult resolveStoreLicense(EntitlementResult localTrial,
                                      bool hasActivePermanentLicense)
{
    if (hasActivePermanentLicense) {
        grantPermanentAccess(localTrial);
        localTrial.message = "InputRack Pro purchase restored.";
    } else if (localTrial.trial) {
        localTrial.message = "InputRack Pro trial: "
            + juce::String(localTrial.trialDaysRemaining) + " day(s) remaining.";
    }
    return localTrial;
}
}
