#include "services/Validation.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace lms::services::validation {

void nonblank(const std::string& v, const char* field) {
    if (v.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument(std::string(field) + " must not be empty");
}

void isoUtc(const std::string& v, const char* field) {
    if (v.size() != 20 || v[4] != '-' || v[7] != '-' || v[10] != 'T' || v[13] != ':' || v[16] != ':' || v.back() != 'Z')
        throw std::invalid_argument(std::string(field) + " must be YYYY-MM-DDTHH:MM:SSZ in UTC");
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19)
            continue;
        if (v[i] < '0' || v[i] > '9')
            throw std::invalid_argument(std::string(field) + " contains invalid date/time characters");
    }
    const int year = std::stoi(v.substr(0, 4)), month = std::stoi(v.substr(5, 2)), day = std::stoi(v.substr(8, 2)),
              hour = std::stoi(v.substr(11, 2)), minute = std::stoi(v.substr(14, 2)),
              second = std::stoi(v.substr(17, 2));
    if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59)
        throw std::invalid_argument(std::string(field) + " contains an out-of-range date/time component");
    if (day < 1 || day > daysInMonth(year, month))
        throw std::invalid_argument(std::string(field) + " contains an invalid calendar date");
}

int daysInMonth(int year, int month) {
    if (month < 1 || month > 12)
        throw std::invalid_argument("Month must be between 1 and 12");
    const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && leap ? 29 : kDays[month - 1];
}

void minimumPasswordLength(const std::string& password) {
    if (password.size() < 10)
        throw std::invalid_argument("Password must contain at least 10 characters");
}

std::string systemUtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

} // namespace lms::services::validation
