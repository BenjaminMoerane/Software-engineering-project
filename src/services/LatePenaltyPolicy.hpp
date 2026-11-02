#pragma once
// Convenience facade over DeadlineEvaluator + the provisional penalty strategy, kept so the Phase 4/5 late-policy
// tests (T-PENALTY boundary tables) exercise exactly the rule MarkingService uses. New code should depend on
// IPenaltyStrategy, not on this class.
#include "services/DeadlineEvaluator.hpp"
#include "services/PenaltyStrategy.hpp"

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
        const ProvisionalLinearPenaltyStrategy strategy;
        const PenaltyResult r = strategy.calculate(elapsedLateDays, maximumMark);
        return {elapsedLateDays, r.penalty, applyPenalty(rawMark, r), DeadlineEvaluator::classify(elapsedLateDays)};
    }
};
} // namespace lms::services
