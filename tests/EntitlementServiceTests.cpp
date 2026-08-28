#include <inputrack/EntitlementService.h>

int main()
{
    inputrack::EntitlementResult free;
    inputrack::EntitlementResult trial{false, true, false, 14, {}};
    inputrack::EntitlementResult permanent{true, false, false, 0, {}};
    if (free.hasProAccess() || !trial.hasProAccess() || !permanent.hasProAccess()) return 3;

    const auto service = inputrack::EntitlementService::create();
    if (!service->state().hasProAccess() || service->state().trial
        || service->state().trialAvailable || service->isBusy()) return 1;
    auto called = false;
    service->refresh(nullptr, [&called](inputrack::EntitlementResult result) {
        called = result.permanent && result.hasProAccess() && result.message.isEmpty();
    });
    return called ? 0 : 2;
}
