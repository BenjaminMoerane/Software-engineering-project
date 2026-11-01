// T-MOD-RELEASE / T-REL: HoD moderation, explicit release and student visibility
// (FR-15..FR-18; UC-03 main, alternative and exception flows; OD-08 behaviour).
#include "TestSupport.hpp"

using namespace lmstest;

class Moderation : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
        assignment_ = newAssignment();
        submission_ = submitAt(kBeforeDeadline, assignment_, student_);
        mark_ = svc().markSubmission(lecturer_, submission_, 80.0, "Good work", "Initial mark");
    }
    int assignment_ = 0, submission_ = 0, mark_ = 0;
};

TEST_F(Moderation, T_MOD_01_approve_records_the_decision_hod_and_reason) {
    svc().moderate(hod_, mark_, true, "Looks fair");
    EXPECT_EQ(scalarText("SELECT decision FROM moderation_decisions"), "Approved");
    EXPECT_EQ(scalar("SELECT hod_id FROM moderation_decisions"), hodId_);
    EXPECT_EQ(scalarText("SELECT reason FROM moderation_decisions"), "Looks fair");
    EXPECT_FALSE(scalarText("SELECT decided_at FROM moderation_decisions").empty());
}

TEST_F(Moderation, T_MOD_02_return_for_correction_sends_the_mark_back_to_the_lecturer) {
    EXPECT_TRUE(svc().submissionsForMarking(lecturer_).empty());
    svc().moderate(hod_, mark_, false, "Please clarify");
    EXPECT_EQ(scalarText("SELECT decision FROM moderation_decisions"), "ReturnedForCorrection");
    EXPECT_EQ(svc().submissionsForMarking(lecturer_).size(), 1U);
    EXPECT_FAILS_WITH(svc().release(hod_, mark_), "cannot be released without latest HoD approval");
}

TEST_F(Moderation, T_MOD_03_reason_is_mandatory_and_the_mark_must_exist) {
    EXPECT_FAILS_WITH(svc().moderate(hod_, mark_, true, ""), "Moderation reason must not be empty");
    EXPECT_FAILS_WITH(svc().moderate(hod_, 9999, true, "r"), "Current mark not found");
    EXPECT_EQ(scalar("SELECT count(*) FROM moderation_decisions"), 0);
}

TEST_F(Moderation, T_MOD_04_the_latest_decision_wins) {
    svc().moderate(hod_, mark_, false, "return");
    svc().moderate(hod_, mark_, true, "approve after discussion");
    EXPECT_NO_THROW(svc().release(hod_, mark_));
}

TEST_F(Moderation, T_MOD_05_release_needs_approval) {
    EXPECT_FAILS_WITH(svc().release(hod_, mark_), "cannot be released without latest HoD approval"); // never moderated
    EXPECT_FAILS_WITH(svc().release(hod_, 9999), "Current mark not found");
    EXPECT_EQ(scalar("SELECT count(*) FROM release_records"), 0);
}

TEST_F(Moderation, T_MOD_06_a_mark_is_released_once) {
    svc().moderate(hod_, mark_, true, "ok");
    svc().release(hod_, mark_);
    EXPECT_ANY_THROW(svc().release(hod_, mark_));
    EXPECT_EQ(scalar("SELECT count(*) FROM release_records"), 1);
    EXPECT_EQ(scalar("SELECT released_by FROM release_records"), hodId_);
}

TEST_F(Moderation, T_MOD_07_approval_alone_does_not_release) {
    svc().moderate(hod_, mark_, true, "ok");
    EXPECT_TRUE(svc().releasedResults(student_).empty()) << "approved but not released stays hidden (UC-03 A2)";
    svc().release(hod_, mark_);
    EXPECT_EQ(svc().releasedResults(student_).size(), 1U);
}

TEST_F(Moderation, T_MOD_08_returned_mark_needs_a_fresh_approval_after_correction) {
    svc().moderate(hod_, mark_, false, "return");
    EXPECT_EQ(svc().markSubmission(lecturer_, submission_, 85.0, "Revised feedback", "Corrected"), mark_);
    EXPECT_FAILS_WITH(svc().release(hod_, mark_), "cannot be released without latest HoD approval");
    svc().moderate(hod_, mark_, true, "now fine");
    EXPECT_NO_THROW(svc().release(hod_, mark_));
    EXPECT_DOUBLE_EQ(svc().releasedResults(student_).front().mark, 85.0);
}

TEST_F(Moderation, T_MOD_09_an_approved_but_unreleased_mark_cannot_be_changed) {
    svc().moderate(hod_, mark_, true, "ok");
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 10.0, "sneaky", "change after approval"),
                      "Mark is approved by the HoD");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(mark_)), 80.0);
    EXPECT_EQ(scalar("SELECT count(*) FROM mark_audit WHERE mark_id=" + std::to_string(mark_)), 1);
    // the supported path: the HoD returns it, the lecturer corrects, the HoD approves again (UC-03 E4)
    svc().moderate(hod_, mark_, false, "returning so it can be corrected");
    EXPECT_NO_THROW(svc().markSubmission(lecturer_, submission_, 75.0, "fixed", "corrected"));
    EXPECT_FAILS_WITH(svc().release(hod_, mark_), "cannot be released without latest HoD approval");
}

TEST_F(Moderation, T_MOD_10_released_marks_cannot_be_changed_or_moderated_pending_OD_08) {
    svc().moderate(hod_, mark_, true, "ok");
    svc().release(hod_, mark_);
    EXPECT_FAILS_WITH(svc().markSubmission(lecturer_, submission_, 90.0, "later", "late correction"), "OD-08");
    EXPECT_FAILS_WITH(svc().moderate(hod_, mark_, false, "too late"), "OD-08");
    EXPECT_DOUBLE_EQ(svc().releasedResults(student_).front().mark, 80.0);
}

TEST_F(Moderation, T_MOD_11_only_the_hod_of_the_department_may_decide) {
    EXPECT_FAILS_WITH(svc().moderate(otherHod_, mark_, true, "r"), "outside HoD department");
    EXPECT_FAILS_WITH(svc().release(otherHod_, mark_), "outside HoD department");
    EXPECT_EQ(scalar("SELECT count(*) FROM moderation_decisions"), 0);
}

TEST_F(Moderation, T_MOD_12_failed_release_does_not_expose_the_result) {
    svc().moderate(hod_, mark_, true, "ok");
    db().execute(
        "CREATE TRIGGER fail_release BEFORE INSERT ON release_records BEGIN SELECT RAISE(ABORT,'forced failure'); END;");
    EXPECT_FAILS_WITH(svc().release(hod_, mark_), "forced failure");
    db().execute("DROP TRIGGER fail_release;");
    EXPECT_TRUE(svc().releasedResults(student_).empty());
    EXPECT_NO_THROW(svc().release(hod_, mark_));
}

// ---- T-REL: what the student sees ----

TEST_F(Moderation, T_REL_01_nothing_is_visible_before_release) {
    EXPECT_TRUE(svc().releasedResults(student_).empty()); // marked, not moderated
    svc().moderate(hod_, mark_, false, "return");
    EXPECT_TRUE(svc().releasedResults(student_).empty()); // returned
}

TEST_F(Moderation, T_REL_02_released_result_shows_final_mark_maximum_and_feedback) {
    svc().moderate(hod_, mark_, true, "ok");
    svc().release(hod_, mark_);
    const auto results = svc().releasedResults(student_);
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0].courseCode, "SCI101");
    EXPECT_EQ(results[0].assignmentTitle, "Lab report");
    EXPECT_DOUBLE_EQ(results[0].mark, 80.0);
    EXPECT_DOUBLE_EQ(results[0].maxMark, 100.0);
    EXPECT_EQ(results[0].feedback, "Good work");
}

TEST_F(Moderation, T_REL_03_the_student_sees_the_penalised_final_mark) {
    const int a = newAssignment(kDeadline, 100.0, "late work");
    const int s = submitAt("2026-10-21T10:00:00Z", a, student2_); // late, day 1
    const int m = svc().markSubmission(lecturer_, s, 80.0, "late but good", "initial");
    svc().moderate(hod_, m, true, "ok");
    svc().release(hod_, m);
    EXPECT_DOUBLE_EQ(svc().releasedResults(student2_).front().mark, 70.0);
}

TEST_F(Moderation, T_REL_04_results_are_private_to_their_owner_and_listed_for_each_assignment) {
    const int a2 = newAssignment(kDeadline, 50.0, "Second lab");
    const int s2 = submitAt(kBeforeDeadline, a2, student_);
    const int m2 = svc().markSubmission(lecturer_, s2, 40.0, "fine", "initial");
    svc().moderate(hod_, mark_, true, "ok");
    svc().release(hod_, mark_);
    svc().moderate(hod_, m2, true, "ok");
    svc().release(hod_, m2);
    EXPECT_EQ(svc().releasedResults(student_).size(), 2U);
    EXPECT_TRUE(svc().releasedResults(student2_).empty());
    EXPECT_TRUE(svc().releasedResults(outsider_).empty());
}
