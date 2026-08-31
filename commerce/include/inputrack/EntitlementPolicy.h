#pragma once

#include <inputrack/EntitlementService.h>

namespace inputrack {
enum class StorePurchaseOutcome {
    purchased,
    cancelled,
    failed,
};

/** Applies a Store purchase response without discarding the user's current access. */
EntitlementResult resolveStorePurchase(EntitlementResult current,
                                       StorePurchaseOutcome outcome);

/** Combines the local trial with the authoritative active Store add-on state. */
EntitlementResult resolveStoreLicense(EntitlementResult localTrial,
                                      bool hasActivePermanentLicense);
}
