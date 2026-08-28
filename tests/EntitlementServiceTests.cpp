#include <inputrack/EntitlementService.h>

int main()
{
    const auto service = inputrack::EntitlementService::create();
    if (!service->isPro() || service->isBusy()) return 1;
    auto called = false;
    service->refresh(nullptr, [&called](inputrack::EntitlementResult result) {
        called = result.pro && result.message.isEmpty();
    });
    return called ? 0 : 2;
}
