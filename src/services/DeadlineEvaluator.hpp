#pragma once
// DeadlineEvaluator (Phase 3 SDD, SOLID/SRP): turns a server-recorded submission timestamp and the applicable
// deadline into timing facts. It has no knowledge of marks or penalties; the penalty strategy is applied later,
// at marking time, so resubmission or display can never apply a deduction twice.
//
// Provisional rule pending Gate 2: lateness = elapsed 24-hour periods after the deadline, rounded up.
#include <cstdint>
#include <optional>
#include <string>

namespace lms::services {

class DeadlineEvaluator {
  public:
    // Seconds since 1970-01-01T00:00:00Z for "YYYY-MM-DDTHH:MM:SS[.fff][Z]". Throws std::invalid_argument otherwise.
    static std::int64_t toEpochSeconds(const std::string& isoUtc);
    // 0 when submittedAt <= deadline, otherwise ceil((submittedAt - deadline) / 24h).
    static int latenessDays(const std::string& submittedAt, const std::string& deadline);
    // "OnTime" (0 days), "LateWithinWindow" (1-3 days) or "BeyondWindow" (> 3 days).
    static const char* classify(int latenessDays);
    // BR-11 (CR1, provisional decision CR1-D2): the applicable deadline is the LATEST of the original deadline, the
    // class-wide extension in force and the student's individual extension, so no extension can shorten the time a
    // student already has. The only place this rule is implemented.
    static std::string applicableDeadline(const std::string& original, const std::optional<std::string>& individual,
                                          const std::optional<std::string>& classWide);
};

} // namespace lms::services
