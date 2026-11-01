// T-ASG / T-EXT: assignment creation validation and individual extensions (FR-06, FR-07, FR-21).
#include "TestSupport.hpp"

#include <cmath>
#include <limits>

using namespace lmstest;

class Assignments : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
    }
};

TEST_F(Assignments, T_ASG_01_valid_assignment_is_created_and_listed) {
    const int id = svc().createAssignment(lecturer_, course_, "Lab 1", "desc", kDeadline, 50.0);
    const auto list = svc().listAssignments(student_);
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].id, id);
    EXPECT_EQ(list[0].courseCode, "SCI101");
    EXPECT_EQ(list[0].title, "Lab 1");
    EXPECT_EQ(list[0].deadline, kDeadline);
    EXPECT_DOUBLE_EQ(list[0].maxMark, 50.0);
    EXPECT_TRUE(list[0].extensionDeadline.empty());
}

TEST_F(Assignments, T_ASG_02_maximum_mark_boundaries) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double bad : {0.0, -0.01, -100.0, nan, inf})
        EXPECT_FAILS_WITH(svc().createAssignment(lecturer_, course_, "T", "d", kDeadline, bad),
                          "Maximum mark must be positive and finite");
    EXPECT_NO_THROW(svc().createAssignment(lecturer_, course_, "Smallest", "d", kDeadline, 0.01));
    EXPECT_NO_THROW(svc().createAssignment(lecturer_, course_, "Large", "d", kDeadline, 1000000.0));
}

TEST_F(Assignments, T_ASG_03_title_is_required) {
    EXPECT_FAILS_WITH(svc().createAssignment(lecturer_, course_, "", "d", kDeadline, 10),
                      "Assignment title must not be empty");
    EXPECT_FAILS_WITH(svc().createAssignment(lecturer_, course_, " \t", "d", kDeadline, 10),
                      "Assignment title must not be empty");
}

TEST_F(Assignments, T_ASG_04_deadline_format_and_calendar_validation) {
    const char* invalid[] = {"",
                             "2026-10-20",
                             "2026-10-20 12:00:00",
                             "2026-10-20T12:00:00",
                             "2026-10-20T12:00:00+02:00",
                             "2026-13-01T00:00:00Z",
                             "2026-00-10T00:00:00Z",
                             "2026-10-00T00:00:00Z",
                             "2026-02-30T00:00:00Z",
                             "2027-02-29T00:00:00Z",
                             "2026-04-31T00:00:00Z",
                             "2026-10-20T24:00:00Z",
                             "2026-10-20T12:60:00Z",
                             "2026-10-20T12:00:60Z",
                             "abcd-ef-ghTij:kl:mnZ",
                             "0000-01-01T00:00:00Z"};
    for (const char* d : invalid)
        EXPECT_ANY_THROW(svc().createAssignment(lecturer_, course_, "T", "x", d, 10)) << d;
    EXPECT_NO_THROW(svc().createAssignment(lecturer_, course_, "Leap", "x", "2028-02-29T23:59:59Z", 10));
    EXPECT_NO_THROW(svc().createAssignment(lecturer_, course_, "Min", "x", "2026-01-01T00:00:00Z", 10));
    EXPECT_NO_THROW(svc().createAssignment(lecturer_, course_, "Dec", "x", "2026-12-31T23:59:59Z", 10));
}

TEST_F(Assignments, T_ASG_05_unknown_course_is_rejected) {
    EXPECT_FAILS_WITH(svc().createAssignment(lecturer_, 9999, "T", "d", kDeadline, 10), "not assigned");
}

// ---- T-EXT ----

TEST_F(Assignments, T_EXT_01_extension_is_shown_only_to_the_affected_student) {
    const int a = newAssignment();
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "Illness");
    const auto mine = svc().listAssignments(student_);
    const auto other = svc().listAssignments(student2_);
    ASSERT_EQ(mine.size(), 1U);
    ASSERT_EQ(other.size(), 1U);
    EXPECT_EQ(mine[0].extensionDeadline, "2026-10-25T12:00:00Z");
    EXPECT_EQ(mine[0].deadline, kDeadline); // the original deadline is unchanged
    EXPECT_TRUE(other[0].extensionDeadline.empty());
    EXPECT_EQ(scalar("SELECT count(*) FROM extensions"), 1);
    EXPECT_EQ(scalarText("SELECT reason FROM extensions"), "Illness");
    EXPECT_EQ(scalar("SELECT authorised_by FROM extensions"), lecturerId_);
}

TEST_F(Assignments, T_EXT_02_extension_changes_the_applicable_deadline_for_submissions) {
    const int a = newAssignment();
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "Illness");
    const int onTime = submitAt("2026-10-24T12:00:00Z", a, student_); // after the original, before the revised deadline
    EXPECT_EQ(lateStatusOf(onTime), "OnTime");
    EXPECT_EQ(latenessOf(onTime), 0);
    EXPECT_EQ(scalarText("SELECT deadline_used FROM submission_versions WHERE submission_id=" + std::to_string(onTime)),
              "2026-10-25T12:00:00Z");
    const int late =
        submitAt("2026-10-24T12:00:00Z", a, student2_); // no extension: 4 days late (2026-10-20T12 -> 24T12)
    EXPECT_EQ(lateStatusOf(late), "BeyondWindow");
}

TEST_F(Assignments, T_EXT_03_lateness_is_measured_from_the_revised_deadline) {
    const int a = newAssignment();
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "Illness");
    const int s = submitAt("2026-10-26T12:00:00Z", a, student_); // exactly one day after the revised deadline
    EXPECT_EQ(latenessOf(s), 1);
    EXPECT_EQ(lateStatusOf(s), "LateWithinWindow");
}

TEST_F(Assignments, T_EXT_04_revised_deadline_must_be_later_than_the_original) {
    const int a = newAssignment();
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, studentId_, kDeadline, "r"),
                      "must be later than original");
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-19T12:00:00Z", "r"),
                      "must be later than original");
    EXPECT_NO_THROW(
        svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-20T12:00:01Z", "r")); // one second later
}

TEST_F(Assignments, T_EXT_05_extension_input_validation) {
    const int a = newAssignment();
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, studentId_, "tomorrow", "r"),
                      "Revised deadline must be");
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", ""),
                      "Extension reason must not be empty");
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, 9999, studentId_, "2026-10-25T12:00:00Z", "r"),
                      "Assignment not found or inactive");
    EXPECT_EQ(scalar("SELECT count(*) FROM extensions"), 0);
}

TEST_F(Assignments, T_EXT_06_student_must_be_enrolled_in_the_assignments_course) {
    const int a = newAssignment();
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, outsiderId_, "2026-10-25T12:00:00Z", "r"),
                      "not enrolled in the assignment course");
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, lecturerId_, "2026-10-25T12:00:00Z", "r"),
                      "not enrolled in the assignment course");
}

TEST_F(Assignments, T_EXT_07_a_second_grant_replaces_the_first) {
    const int a = newAssignment();
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "first");
    svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-27T12:00:00Z", "second");
    EXPECT_EQ(scalar("SELECT count(*) FROM extensions"), 1);
    EXPECT_EQ(svc().listAssignments(student_)[0].extensionDeadline, "2026-10-27T12:00:00Z");
    EXPECT_EQ(scalarText("SELECT reason FROM extensions"), "second");
}

TEST_F(Assignments, T_EXT_08_inactive_assignment_cannot_be_extended_or_submitted) {
    const int a = newAssignment();
    db().execute("UPDATE assignments SET active=0 WHERE assignment_id=" + std::to_string(a));
    EXPECT_FAILS_WITH(svc().grantIndividualExtension(lecturer_, a, studentId_, "2026-10-25T12:00:00Z", "r"),
                      "Assignment not found or inactive");
    EXPECT_FAILS_WITH(svc().submit(student_, a, "ref"), "Assignment not found or inactive");
    EXPECT_TRUE(svc().listAssignments(student_).empty());
}
