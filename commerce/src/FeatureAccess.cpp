#include <inputrack/FeatureAccess.h>

namespace inputrack {
bool hasFeatureAccess(ProductFeature feature,
                      const EntitlementResult& entitlement) noexcept
{
    switch (feature) {
        case ProductFeature::rackPresets:
        case ProductFeature::windowsStartup:
            return true;
        case ProductFeature::workflowProfiles:
        case ProductFeature::automaticProfiles:
        case ProductFeature::globalHotkeys:
            return entitlement.hasProAccess();
    }
    return false;
}
}
