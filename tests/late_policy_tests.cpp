#include "services/LatePenaltyPolicy.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void mustFail(F action, const std::string& message) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    require(failed, message);
}
}

int main() {
    using lms::services::LatePenaltyPolicy;
    try {
        auto onTime = LatePenaltyPolicy::evaluate(80.0, 100.0, 0);
        require(onTime.lateDays == 0 && onTime.penalty == 0.0 && onTime.finalMark == 80.0,
                "on-time submission must not incur a penalty");
        auto oneDay = LatePenaltyPolicy::evaluate(80.0, 100.0, 1);
        require(oneDay.penalty == 10.0 && oneDay.finalMark == 70.0,
                "one late day must deduct ten percent of maximum mark under provisional rule");
        auto threeDays = LatePenaltyPolicy::evaluate(15.0, 100.0, 3);
        require(threeDays.penalty == 30.0 && threeDays.finalMark == 0.0,
                "three-day penalty must clamp a low raw mark to zero");
        auto fourDays = LatePenaltyPolicy::evaluate(100.0, 100.0, 4);
        require(std::string(fourDays.status) == "BeyondWindow" && fourDays.finalMark == 0.0,
                "more than three late days must produce zero under provisional rule");
        mustFail([] { LatePenaltyPolicy::evaluate(-0.01, 100.0, 0); }, "negative raw mark rejected");
        mustFail([] { LatePenaltyPolicy::evaluate(100.01, 100.0, 0); }, "raw mark above maximum rejected");
        mustFail([] { LatePenaltyPolicy::evaluate(1.0, 0.0, 0); }, "nonpositive maximum rejected");
        mustFail([] { LatePenaltyPolicy::evaluate(1.0, 100.0, -1); }, "negative lateness rejected");
        mustFail([] { LatePenaltyPolicy::evaluate(std::numeric_limits<double>::quiet_NaN(), 100.0, 0); }, "NaN raw mark rejected");
        mustFail([] { LatePenaltyPolicy::evaluate(1.0, std::numeric_limits<double>::infinity(), 0); }, "infinite maximum rejected");
        std::cout << "PASS: late-policy arithmetic, zero floor, three-day boundary, beyond-window status, invalid and non-finite inputs (policy remains provisional pending Gate 2)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
