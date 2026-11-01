#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lms::services {
struct PenaltyOutcome {
    int lateDays{};
    double penalty{};
    double finalMark{};
    const char* status{"OnTime"};
};
// Provisional interpretation only. Gate 2 must approve calendar-vs-24-hour counting,
// exact boundary semantics, and rounding before this policy is treated as authoritative.
class LatePenaltyPolicy {
  public:
    static PenaltyOutcome evaluate(double rawMark, double maximumMark, int elapsedLateDays) {
        if (!std::isfinite(rawMark) || !std::isfinite(maximumMark) || maximumMark <= 0.0 || rawMark < 0.0 ||
            rawMark > maximumMark || elapsedLateDays < 0)
            throw std::invalid_argument("Invalid mark or lateness input");
        if (elapsedLateDays == 0)
            return {0, 0.0, rawMark, "OnTime"};
        if (elapsedLateDays > 3)
            return {elapsedLateDays, maximumMark, 0.0, "BeyondWindow"};
        const double penalty = static_cast<double>(elapsedLateDays) * maximumMark * 0.10;
        return {elapsedLateDays, penalty, std::max(0.0, rawMark - penalty), "LateWithinWindow"};
    }
};
} // namespace lms::services
