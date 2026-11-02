#pragma once
// Strategy pattern for the late-penalty rule (Phase 3 SDD, Pattern 1).
//
// IPenaltyStrategy::calculate(latenessDays, maximumMark) returns the deduction only. MarkingService applies it once,
// at marking time, and stores rawMark, penaltyAmount and finalMark separately so the arithmetic can be audited.
//
// ProvisionalLinearPenaltyStrategy is deliberately named "provisional": the exact interpretation (calendar days vs
// elapsed 24-hour periods, rounding) is an open Gate 2 decision. Swapping in an approved rule means adding a new
// strategy class, not editing MarkingService (Open/Closed principle).
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lms::services {

struct PenaltyResult {
    double penalty{};   // amount deducted, in marks
    bool zeroed{false}; // true when the work is beyond the late window and must receive zero
};

class IPenaltyStrategy {
  public:
    IPenaltyStrategy() = default;
    virtual ~IPenaltyStrategy() = default;
    IPenaltyStrategy(const IPenaltyStrategy&) = default;
    IPenaltyStrategy& operator=(const IPenaltyStrategy&) = default;
    IPenaltyStrategy(IPenaltyStrategy&&) = default;
    IPenaltyStrategy& operator=(IPenaltyStrategy&&) = default;
    virtual PenaltyResult calculate(int latenessDays, double maximumMark) const = 0;
};

// BR-05 as currently interpreted: 10% of the maximum mark per late day for days 1-3; zero after day 3.
class ProvisionalLinearPenaltyStrategy final : public IPenaltyStrategy {
  public:
    static constexpr int kWindowDays = 3;
    static constexpr double kRatePerDay = 0.10;

    PenaltyResult calculate(int latenessDays, double maximumMark) const override {
        if (!std::isfinite(maximumMark) || maximumMark <= 0.0 || latenessDays < 0)
            throw std::invalid_argument("Invalid mark or lateness input");
        if (latenessDays > kWindowDays)
            return {maximumMark, true};
        return {static_cast<double>(latenessDays) * maximumMark * kRatePerDay, false};
    }
};

// Applies a strategy to a raw mark: finalMark = max(0, raw - penalty), or 0 beyond the window.
inline double applyPenalty(double rawMark, const PenaltyResult& result) {
    return result.zeroed ? 0.0 : std::max(0.0, rawMark - result.penalty);
}

} // namespace lms::services
