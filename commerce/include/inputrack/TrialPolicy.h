#pragma once

#include <cstdint>
#include <optional>

namespace inputrack {
struct TrialState {
    bool active{};
    bool available{};
    int daysRemaining{};
};

/** Evaluates the application-managed trial without accessing persistent state. */
TrialState evaluateTrialState(std::optional<std::uint64_t> startedUnix,
                              std::uint64_t nowUnix,
                              int durationDays = 14) noexcept;
}
