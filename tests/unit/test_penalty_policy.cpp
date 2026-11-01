// T-PEN: late-penalty arithmetic. Provisional rule (OD-01..OD-04): 10 points of the maximum per late day,
// three days maximum, zero afterwards. Equivalence classes by lateness day: 0 | 1..3 | >3 | invalid.
#include "services/LatePenaltyPolicy.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

using lms::services::LatePenaltyPolicy;

TEST(PenaltyPolicy, T_PEN_01_on_time_has_no_penalty) {
    const auto r = LatePenaltyPolicy::evaluate(80.0, 100.0, 0);
    EXPECT_EQ(r.lateDays, 0);
    EXPECT_DOUBLE_EQ(r.penalty, 0.0);
    EXPECT_DOUBLE_EQ(r.finalMark, 80.0);
    EXPECT_STREQ(r.status, "OnTime");
}

TEST(PenaltyPolicy, T_PEN_02_one_two_three_late_days) {
    const struct {
        int days;
        double penalty, finalMark;
    } rows[] = {{1, 10.0, 70.0}, {2, 20.0, 60.0}, {3, 30.0, 50.0}};
    for (const auto& row : rows) {
        const auto r = LatePenaltyPolicy::evaluate(80.0, 100.0, row.days);
        EXPECT_DOUBLE_EQ(r.penalty, row.penalty) << "days=" << row.days;
        EXPECT_DOUBLE_EQ(r.finalMark, row.finalMark) << "days=" << row.days;
        EXPECT_STREQ(r.status, "LateWithinWindow");
    }
}

TEST(PenaltyPolicy, T_PEN_03_beyond_three_days_is_zero) {
    for (int days : {4, 5, 30, 100000}) {
        const auto r = LatePenaltyPolicy::evaluate(100.0, 100.0, days);
        EXPECT_DOUBLE_EQ(r.finalMark, 0.0) << days;
        EXPECT_DOUBLE_EQ(r.penalty, 100.0) << days;
        EXPECT_STREQ(r.status, "BeyondWindow");
    }
}

TEST(PenaltyPolicy, T_PEN_04_floor_at_zero_never_negative) {
    EXPECT_DOUBLE_EQ(LatePenaltyPolicy::evaluate(15.0, 100.0, 3).finalMark, 0.0);
    EXPECT_DOUBLE_EQ(LatePenaltyPolicy::evaluate(0.0, 100.0, 1).finalMark, 0.0);
    EXPECT_DOUBLE_EQ(LatePenaltyPolicy::evaluate(10.0, 100.0, 1).finalMark, 0.0); // exactly the penalty
}

TEST(PenaltyPolicy, T_PEN_05_penalty_applied_once_and_scales_with_maximum) {
    const auto r = LatePenaltyPolicy::evaluate(40.0, 50.0, 2); // 2 days x 10% of 50
    EXPECT_DOUBLE_EQ(r.penalty, 10.0);
    EXPECT_DOUBLE_EQ(r.finalMark, 30.0);
}

TEST(PenaltyPolicy, T_PEN_06_mark_boundaries_accepted) {
    EXPECT_NO_THROW(LatePenaltyPolicy::evaluate(0.0, 100.0, 0));
    EXPECT_NO_THROW(LatePenaltyPolicy::evaluate(100.0, 100.0, 0));
}

TEST(PenaltyPolicy, T_PEN_07_invalid_inputs_rejected) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_THROW(LatePenaltyPolicy::evaluate(-0.01, 100.0, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(100.01, 100.0, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(1.0, 0.0, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(1.0, -5.0, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(1.0, 100.0, -1), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(nan, 100.0, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(1.0, nan, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(1.0, inf, 0), std::invalid_argument);
    EXPECT_THROW(LatePenaltyPolicy::evaluate(inf, 100.0, 0), std::invalid_argument);
}
