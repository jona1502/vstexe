#pragma once

#include <inputrack/EntitlementService.h>

namespace inputrack {
enum class ProductFeature {
    rackPresets,
    windowsStartup,
    workflowProfiles,
    automaticProfiles,
    globalHotkeys,
};

/** Central Free/Pro boundary used by the desktop UI and commerce tests. */
bool hasFeatureAccess(ProductFeature feature,
                      const EntitlementResult& entitlement) noexcept;
}
