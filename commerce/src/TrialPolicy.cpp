#include <inputrack/TrialPolicy.h>

namespace inputrack {
TrialState evaluateTrialState(std::optional<std::uint64_t> startedUnix,
                              std::uint64_t nowUnix,
                              int durationDays) noexcept
{
    if (!startedUnix.has_value()) return {false, true, 0};
    if (durationDays <= 0) return {false, false, 0};

    constexpr std::uint64_t secondsPerDay = 24ULL * 60ULL * 60ULL;
    const auto durationSeconds = static_cast<std::uint64_t>(durationDays) * secondsPerDay;
    const auto elapsed = nowUnix > *startedUnix ? nowUnix - *startedUnix : 0;
    if (elapsed >= durationSeconds) return {false, false, 0};

    const auto remaining = durationSeconds - elapsed;
    return {true, false, static_cast<int>((remaining + secondsPerDay - 1) / secondsPerDay)};
}
}
