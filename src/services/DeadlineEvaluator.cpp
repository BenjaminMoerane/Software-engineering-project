#include "services/DeadlineEvaluator.hpp"
#include "services/PenaltyStrategy.hpp"
#include "services/Validation.hpp"

#include <stdexcept>

namespace lms::services {
namespace {
int digits(const std::string& s, std::size_t pos, std::size_t count) {
    int value = 0;
    for (std::size_t i = pos; i < pos + count; ++i) {
        if (i >= s.size() || s[i] < '0' || s[i] > '9')
            throw std::invalid_argument("Timestamp is not a valid UTC date/time: " + s);
        value = value * 10 + (s[i] - '0');
    }
    return value;
}
// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil algorithm).
std::int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
} // namespace

std::int64_t DeadlineEvaluator::toEpochSeconds(const std::string& s) {
    if (s.size() < 19 || s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != ' ') || s[13] != ':' || s[16] != ':')
        throw std::invalid_argument("Timestamp is not a valid UTC date/time: " + s);
    const int year = digits(s, 0, 4), month = digits(s, 5, 2), day = digits(s, 8, 2);
    const int hour = digits(s, 11, 2), minute = digits(s, 14, 2), second = digits(s, 17, 2);
    std::size_t i = 19;
    if (i < s.size() && s[i] == '.') { // fractional seconds are ignored (truncated), as SQLite strftime('%s') does
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9')
            ++i;
    }
    if (i < s.size() && s[i] == 'Z')
        ++i;
    // Phase 6 review CR-04: reject impossible calendar dates (e.g. 2026-02-30) instead of normalising them forward.
    if (i != s.size() || month < 1 || month > 12 || day < 1 || day > validation::daysInMonth(year, month) ||
        hour > 23 || minute > 59 || second > 59)
        throw std::invalid_argument("Timestamp is not a valid UTC date/time: " + s);
    return daysFromCivil(year, month, day) * 86400 + std::int64_t{hour} * 3600 + std::int64_t{minute} * 60 + second;
}

int DeadlineEvaluator::latenessDays(const std::string& submittedAt, const std::string& deadline) {
    const std::int64_t late = toEpochSeconds(submittedAt) - toEpochSeconds(deadline);
    if (late <= 0)
        return 0;
    return static_cast<int>((late + 86399) / 86400);
}

const char* DeadlineEvaluator::classify(int days) {
    if (days <= 0)
        return "OnTime";
    return days <= ProvisionalLinearPenaltyStrategy::kWindowDays ? "LateWithinWindow" : "BeyondWindow";
}

std::string DeadlineEvaluator::applicableDeadline(const std::string& original,
                                                  const std::optional<std::string>& individual,
                                                  const std::optional<std::string>& classWide) {
    std::string best = original;
    std::int64_t bestAt = toEpochSeconds(original);
    for (const auto* candidate : {&individual, &classWide}) {
        if (!candidate->has_value())
            continue;
        const std::int64_t at = toEpochSeconds(**candidate);
        if (at > bestAt) {
            bestAt = at;
            best = **candidate;
        }
    }
    return best;
}

} // namespace lms::services
