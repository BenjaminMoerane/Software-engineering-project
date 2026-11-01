// T-MARK-RANGE / T-MARK / T-AUDIT-TX: marking, feedback and the mark audit log
// (FR-13, FR-14, NFR-06; UC-02 main, alternative and exception flows).
#include "TestSupport.hpp"

#include <cmath>
#include <limits>

using namespace lmstest;

class Marking : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
        assignment_ = newAssignment();
        submission_ = submitAt(kBeforeDeadline, assignment_, student_);
    }
    int auditRows(int markId) {
        return scalar("SELECT count(*) FROM mark_audit WHERE mark_id=" + std::to_string(markId));
    }
    int assignment_ = 0, submission_ = 0;
};

TEST_F(Marking, T_MARK_01_first_mark_and_feedback_are_saved_and_pending_moderation) {
    const int mark = svc().markSubmission(lecturer_, submission_, 80.0, "Good structure", "Initial mark");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT raw_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 80.0);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 80.0);
    EXPECT_EQ(scalarText("SELECT body FROM feedback WHERE mark_id=" + std::to_string(mark)), "Good structure");
    EXPECT_EQ(scalar("SELECT entered_by FROM marks WHERE mark_id=" + std::to_string(mark)), lecturerId_);
    EXPECT_EQ(scalar("SELECT count(*) FROM moderation_decisions"), 0) << "pending HoD moderation";
    EXPECT_TRUE(svc().releasedResults(student_).empty()) << "student cannot see the mark yet";
}

TEST_F(Marking, T_MARK_02_range_boundaries_for_the_raw_mark) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double bad : {-0.01, -1.0, 100.01, 1000.0, nan, inf, -inf}) {
        SCOPED_TRACE(bad);
        EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, bad, "fb", "r"),
                          "Raw mark must be between zero and maximum mark");
    }
    EXPECT_EQ(scalar("SELECT count(*) FROM marks"), 0) << "rejected marks are not saved";
    const int mark = svc().markSubmission(lecturer_, submission_, 0.0, "fb", "zero"); // lower bound
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 0.0);
    svc().markSubmission(lecturer_, submission_, 100.0, "fb", "max"); // upper bound
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 100.0);
    svc().markSubmission(lecturer_, submission_, 99.99, "fb", "just below max");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 99.99);
}

TEST_F(Marking, T_MARK_03_feedback_and_reason_are_mandatory) {
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 50, "", "r"), "Feedback must not be empty");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 50, "  ", "r"), "Feedback must not be empty");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 50, "fb", ""),
                      "Mark-change reason must not be empty");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 50, "fb", "\n"),
                      "Mark-change reason must not be empty");
    EXPECT_EQ(scalar("SELECT count(*) FROM marks"), 0);
    EXPECT_EQ(scalar("SELECT count(*) FROM mark_audit"), 0);
}

TEST_F(Marking, T_MARK_04_unknown_submission_is_rejected) {
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, 9999, 50, "fb", "r"), "Submission not found");
}

TEST_F(Marking, T_MARK_05_one_day_late_work_loses_ten_percent_of_the_maximum) {
    const int a = newAssignment(kDeadline, 100.0, "late work");
    const int s = submitAt("2026-10-20T20:00:00Z", a, student2_); // 8 hours late -> day 1
    const int mark = svc().markSubmission(lecturer_, s, 80.0, "fb", "initial");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT raw_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 80.0);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT penalty_amount FROM marks WHERE mark_id=" + std::to_string(mark)), 10.0);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark)), 70.0);
    EXPECT_DOUBLE_EQ(svc().submissionsForMarking(lecturer_).front().maxMark, 100.0);
}

TEST_F(Marking, T_MARK_06_three_days_late_and_beyond_the_window) {
    const int a3 = newAssignment(kDeadline, 50.0, "three days");
    const int s3 = submitAt("2026-10-23T12:00:00Z", a3, student2_); // exactly 72 hours
    const int m3 = svc().markSubmission(lecturer_, s3, 40.0, "fb", "r");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT penalty_amount FROM marks WHERE mark_id=" + std::to_string(m3)), 15.0);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(m3)), 25.0);
    const int a4 = newAssignment(kDeadline, 50.0, "four days");
    const int s4 = submitAt("2026-10-23T12:00:01Z", a4, student2_);
    const int m4 = svc().markSubmission(lecturer_, s4, 50.0, "fb", "r");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(m4)), 0.0);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT raw_mark FROM marks WHERE mark_id=" + std::to_string(m4)), 50.0)
        << "raw mark is kept";
}

TEST_F(Marking, T_MARK_07_correction_before_moderation_updates_in_place_and_replaces_feedback) {
    const int mark = svc().markSubmission(lecturer_, submission_, 60.0, "first feedback", "initial");
    const int again = svc().markSubmission(lecturer_, submission_, 65.0, "better feedback", "recount");
    EXPECT_EQ(mark, again);
    EXPECT_EQ(scalar("SELECT count(*) FROM marks WHERE submission_id=" + std::to_string(submission_)), 1);
    EXPECT_EQ(scalar("SELECT count(*) FROM feedback WHERE mark_id=" + std::to_string(mark)), 1);
    EXPECT_EQ(scalarText("SELECT body FROM feedback WHERE mark_id=" + std::to_string(mark)), "better feedback");
}

TEST_F(Marking, T_MARK_08_marked_work_leaves_the_lecturer_queue) {
    ASSERT_EQ(svc().submissionsForMarking(lecturer_).size(), 1U);
    svc().markSubmission(lecturer_, submission_, 60.0, "fb", "initial");
    EXPECT_TRUE(svc().submissionsForMarking(lecturer_).empty());
}

// ---- T-AUDIT-TX ----

TEST_F(Marking, T_AUDIT_01_a_first_mark_has_no_old_value) {
    const int mark = svc().markSubmission(lecturer_, submission_, 80.0, "fb", "Initial mark");
    ASSERT_EQ(auditRows(mark), 1);
    const std::string id = std::to_string(mark);
    EXPECT_EQ(scalar("SELECT old_mark IS NULL FROM mark_audit WHERE mark_id=" + id), 1);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT new_mark FROM mark_audit WHERE mark_id=" + id), 80.0);
    EXPECT_EQ(scalar("SELECT actor_id FROM mark_audit WHERE mark_id=" + id), lecturerId_);
    EXPECT_EQ(scalarText("SELECT reason FROM mark_audit WHERE mark_id=" + id), "Initial mark");
    EXPECT_FALSE(scalarText("SELECT changed_at FROM mark_audit WHERE mark_id=" + id).empty());
}

TEST_F(Marking, T_AUDIT_02_every_change_records_actor_time_old_new_and_reason) {
    const int mark = svc().markSubmission(lecturer_, submission_, 60.0, "fb", "initial");
    svc().markSubmission(lecturer_, submission_, 65.0, "fb", "recount");
    svc().markSubmission(lecturer_, submission_, 70.5, "fb", "second recount");
    ASSERT_EQ(auditRows(mark), 3);
    const std::string id = std::to_string(mark);
    EXPECT_DOUBLE_EQ(
        scalarReal("SELECT old_mark FROM mark_audit WHERE mark_id=" + id + " ORDER BY audit_id DESC LIMIT 1"), 65.0);
    EXPECT_DOUBLE_EQ(
        scalarReal("SELECT new_mark FROM mark_audit WHERE mark_id=" + id + " ORDER BY audit_id DESC LIMIT 1"), 70.5);
    EXPECT_EQ(scalarText("SELECT reason FROM mark_audit WHERE mark_id=" + id + " ORDER BY audit_id DESC LIMIT 1"),
              "second recount");
    EXPECT_EQ(scalar("SELECT count(*) FROM mark_audit WHERE mark_id=" + id +
                     " AND changed_at<>'' AND actor_id=" + std::to_string(lecturerId_)),
              3);
}

TEST_F(Marking, T_AUDIT_03_audit_old_and_new_are_final_marks_after_penalty) {
    const int a = newAssignment(kDeadline, 100.0, "late");
    const int s = submitAt("2026-10-20T20:00:00Z", a, student2_);
    const int mark = svc().markSubmission(lecturer_, s, 80.0, "fb", "initial");
    svc().markSubmission(lecturer_, s, 90.0, "fb", "revised");
    const std::string id = std::to_string(mark);
    EXPECT_DOUBLE_EQ(
        scalarReal("SELECT old_mark FROM mark_audit WHERE mark_id=" + id + " ORDER BY audit_id DESC LIMIT 1"), 70.0);
    EXPECT_DOUBLE_EQ(
        scalarReal("SELECT new_mark FROM mark_audit WHERE mark_id=" + id + " ORDER BY audit_id DESC LIMIT 1"), 80.0);
}

TEST_F(Marking, T_AUDIT_04_failed_audit_write_rolls_back_a_first_mark) {
    db().execute(
        "CREATE TRIGGER fail_audit BEFORE INSERT ON mark_audit BEGIN SELECT RAISE(ABORT,'forced audit failure'); END;");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 80.0, "fb", "r"), "forced audit failure");
    db().execute("DROP TRIGGER fail_audit;");
    EXPECT_EQ(scalar("SELECT count(*) FROM marks"), 0);
    EXPECT_EQ(scalar("SELECT count(*) FROM feedback"), 0);
    EXPECT_EQ(scalar("SELECT count(*) FROM mark_audit"), 0);
    EXPECT_NO_THROW(svc().markSubmission(lecturer_, submission_, 80.0, "fb", "retry")); // recovered
}

TEST_F(Marking, T_AUDIT_05_failed_audit_write_leaves_an_existing_mark_and_feedback_unchanged) {
    const int mark = svc().markSubmission(lecturer_, submission_, 60.0, "original feedback", "initial");
    db().execute(
        "CREATE TRIGGER fail_audit BEFORE INSERT ON mark_audit BEGIN SELECT RAISE(ABORT,'forced audit failure'); END;");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 90.0, "new feedback", "r"), "forced audit failure");
    db().execute("DROP TRIGGER fail_audit;");
    const std::string id = std::to_string(mark);
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + id), 60.0);
    EXPECT_EQ(scalarText("SELECT body FROM feedback WHERE mark_id=" + id), "original feedback");
    EXPECT_EQ(auditRows(mark), 1);
}

TEST_F(Marking, T_AUDIT_06_rejected_changes_leave_no_audit_row) {
    const int mark = svc().markSubmission(lecturer_, submission_, 60.0, "fb", "initial");
    EXPECT_ANY_THROW(svc().markSubmission(lecturer_, submission_, 500.0, "fb", "too big"));
    EXPECT_ANY_THROW(svc().markSubmission(lecturer_, submission_, 50.0, "fb", ""));
    EXPECT_EQ(auditRows(mark), 1);
}
