// T-CR1: class-wide extensions (change request CR1, release v1.1; FR-23, BR-11, extended FR-21).
// Provisional decisions under test: CR1-D1 (whole class = all enrolled students), CR1-D2 (latest deadline wins),
// CR1-D3 (class-wide may only move later; history kept), CR1-D4 (existing submissions keep their deadline snapshot).
#include "TestSupport.hpp"

#include "services/DeadlineEvaluator.hpp"

#include <fstream>
#include <sstream>

using namespace lmstest;
using lms::services::DeadlineEvaluator;

namespace {
constexpr const char* kClassDeadline = "2026-10-23T12:00:00Z"; // original kDeadline + 3 days
constexpr const char* kIndividualEarly = "2026-10-21T12:00:00Z";
constexpr const char* kIndividualLate = "2026-10-25T12:00:00Z";
} // namespace

class ClassExtensions : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
        assignment_ = newAssignment();
    }
    int assignment_ = 0;

    std::string classDeadlineShownTo(const Session& who) {
        for (const auto& a : svc().listAssignments(who))
            if (a.id == assignment_)
                return a.classExtensionDeadline;
        return "<assignment not listed>";
    }
};

TEST_F(ClassExtensions, T_CR1_01_class_extension_is_shown_to_every_enrolled_student_and_the_lecturer) {
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Exam week clash");
    EXPECT_EQ(classDeadlineShownTo(student_), kClassDeadline);
    EXPECT_EQ(classDeadlineShownTo(student2_), kClassDeadline);
    EXPECT_EQ(classDeadlineShownTo(lecturer_), kClassDeadline);
    // A student enrolled after the extension was granted is covered too (CR1-D1).
    svc().enrolStudent(admin_, course_, outsiderId_);
    EXPECT_EQ(classDeadlineShownTo(outsider_), kClassDeadline);
    // The individual-extension field is untouched (FR-21 unchanged).
    for (const auto& a : svc().listAssignments(student_))
        EXPECT_TRUE(a.extensionDeadline.empty());
}

TEST_F(ClassExtensions, T_CR1_02_submission_before_class_deadline_is_on_time_after_it_is_late) {
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Exam week clash");
    const int onTime = submitAt("2026-10-22T12:00:00Z", assignment_, student_); // after original, before class
    EXPECT_EQ(lateStatusOf(onTime), "OnTime");
    EXPECT_EQ(latenessOf(onTime), 0);
    const int boundary = submitAt(kClassDeadline, assignment_, student2_); // exactly at the class deadline
    EXPECT_EQ(lateStatusOf(boundary), "OnTime");
    svc().enrolStudent(admin_, course_, outsiderId_);
    const int late = submitAt("2026-10-23T12:00:01Z", assignment_, outsider_); // one second after
    EXPECT_EQ(lateStatusOf(late), "LateWithinWindow");
    EXPECT_EQ(latenessOf(late), 1);
}

TEST_F(ClassExtensions, T_CR1_03_latest_deadline_wins_between_individual_and_class_extensions) {
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Class");
    // Earlier individual extension does not reduce the class extension for that student.
    svc().grantIndividualExtension(lecturer_, assignment_, studentId_, kIndividualEarly, "Illness");
    const int a = submitAt("2026-10-22T12:00:00Z", assignment_, student_);
    EXPECT_EQ(lateStatusOf(a), "OnTime");
    // Later individual extension beats the class extension for that student only.
    svc().grantIndividualExtension(lecturer_, assignment_, student2Id_, kIndividualLate, "Bereavement");
    const int b = submitAt("2026-10-24T12:00:00Z", assignment_, student2_);
    EXPECT_EQ(lateStatusOf(b), "OnTime");
    EXPECT_EQ(scalarText("SELECT deadline_used FROM submission_versions WHERE is_current=1 AND submission_id=" +
                         std::to_string(b)),
              kIndividualLate);
}

TEST_F(ClassExtensions, T_CR1_04_class_extension_can_only_move_later_and_every_change_is_kept) {
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "First");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, "2026-10-22T12:00:00Z", "Shorten"),
                      "can only move the deadline later");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Same"),
                      "can only move the deadline later");
    svc().grantClassExtension(lecturer_, assignment_, "2026-10-24T12:00:00Z", "Second");
    EXPECT_EQ(classDeadlineShownTo(student_), "2026-10-24T12:00:00Z");
    EXPECT_EQ(scalar("SELECT count(*) FROM class_extensions WHERE assignment_id=" + std::to_string(assignment_)), 2);
    EXPECT_EQ(scalar("SELECT count(DISTINCT authorised_by) FROM class_extensions WHERE authorised_by=" +
                     std::to_string(lecturerId_)),
              1);
    EXPECT_EQ(scalarText("SELECT reason FROM class_extensions ORDER BY class_extension_id LIMIT 1"), "First");
}

TEST_F(ClassExtensions, T_CR1_05_existing_submissions_keep_their_recorded_deadline_snapshot) {
    const int late = submitAt("2026-10-21T12:00:00Z", assignment_, student_); // 1 day late under v1.0 rules
    ASSERT_EQ(lateStatusOf(late), "LateWithinWindow");
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Granted after a submission");
    EXPECT_EQ(lateStatusOf(late), "LateWithinWindow"); // CR1-D4: not rewritten
    EXPECT_EQ(latenessOf(late), 1);
    // The penalty applied at marking uses the snapshot too: 80 - 10% of 100 = 70.
    const int markId = svc().markSubmission(lecturer_, late, 80.0, "Feedback", "Initial mark");
    EXPECT_DOUBLE_EQ(scalarReal("SELECT final_mark FROM marks WHERE mark_id=" + std::to_string(markId)), 70.0);
}

TEST_F(ClassExtensions, T_CR1_06_only_the_assigned_lecturer_can_grant_a_class_extension) {
    for (const Session* who : {&admin_, &hod_, &student_})
        EXPECT_FAILS_WITH(svc().grantClassExtension(*who, assignment_, kClassDeadline, "r"), "role is not permitted");
    EXPECT_FAILS_WITH(svc().grantClassExtension(otherLecturer_, assignment_, kClassDeadline, "r"),
                      "lecturer is not assigned to this course");
    EXPECT_EQ(scalar("SELECT count(*) FROM class_extensions"), 0);
}

TEST_F(ClassExtensions, T_CR1_07_input_validation) {
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, kDeadline, "r"),
                      "Revised deadline must be later than original deadline");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, "2026-10-19T12:00:00Z", "r"),
                      "Revised deadline must be later than original deadline");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, "2026-02-30T12:00:00Z", "r"),
                      "invalid calendar date");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, "2026-10-23", "r"), "YYYY-MM-DDTHH:MM:SSZ");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "  "), "must not be empty");
    EXPECT_FAILS_WITH(svc().grantClassExtension(lecturer_, 9999, kClassDeadline, "r"), "Assignment not found");
    EXPECT_EQ(scalar("SELECT count(*) FROM class_extensions"), 0);
}

TEST_F(ClassExtensions, T_CR1_08_resubmission_is_allowed_until_the_class_deadline) {
    svc().grantClassExtension(lecturer_, assignment_, kClassDeadline, "Class");
    const int id = submitAt("2026-10-21T12:00:00Z", assignment_, student_);
    EXPECT_NO_THROW(submitAt("2026-10-23T11:59:59Z", assignment_, student_)); // version 2, before class deadline
    EXPECT_EQ(scalar("SELECT current_version_no FROM submissions WHERE submission_id=" + std::to_string(id)), 2);
    EXPECT_FAILS_WITH(submitAt("2026-10-23T12:00:01Z", assignment_, student_),
                      "Resubmission is only permitted before the applicable deadline");
}

TEST_F(ClassExtensions, T_CR1_09_a_v1_0_database_is_migrated_without_data_loss) {
    // Build a database with the exact v1.0 schema (tests/fixtures/schema_v1_0.sql = `git show
    // v1.0:database_schema.sql`) and v1.0 data, then open it with v1.1.
    svc_.reset();
    db_.reset();
    const std::string oldPath = (dir_ / "v1_0.sqlite3").string();
    {
        std::ifstream in(LMS_V1_0_SCHEMA_FILE);
        ASSERT_TRUE(in) << LMS_V1_0_SCHEMA_FILE;
        std::ostringstream schema;
        schema << in.rdbuf();
        lms::data::Database old(oldPath);
        old.execute(schema.str());
        old.execute("INSERT INTO departments(name) VALUES('Science')");
        old.execute("INSERT INTO users(username,password_hash,password_salt,role,full_name) "
                    "VALUES('admin','00','00','Administrator','A')");
        old.execute("INSERT INTO courses(department_id,course_code,title) VALUES(1,'SCI101','Science')");
        old.execute("INSERT INTO assignments(course_id,title,description,deadline,max_mark,created_by) "
                    "VALUES(1,'Lab','d','2026-10-20T12:00:00Z',100,1)");
    }
    dbPath_ = oldPath;
    openDatabase(); // v1.1 initialiseSchema() on the v1.0 file
    EXPECT_EQ(scalar("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='class_extensions'"), 1);
    EXPECT_EQ(scalar("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"), 15);
    EXPECT_EQ(scalar("SELECT count(*) FROM departments"), 1);
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), 1);
    EXPECT_EQ(scalarText("SELECT deadline FROM assignments WHERE assignment_id=1"), "2026-10-20T12:00:00Z");
    EXPECT_EQ(scalarText("PRAGMA integrity_check"), "ok");
    EXPECT_EQ(scalar("SELECT count(*) FROM pragma_foreign_key_check"), 0);
    reopen(); // running the schema again is a no-op
    EXPECT_EQ(scalar("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"), 15);
}

TEST(DeadlineResolver, T_CR1_10_applicable_deadline_is_the_latest_of_the_three) {
    const std::string orig = "2026-10-20T12:00:00Z";
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::nullopt, std::nullopt), orig); // v1.0 behaviour
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::string(kIndividualEarly), std::nullopt),
              kIndividualEarly);
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::nullopt, std::string(kClassDeadline)), kClassDeadline);
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::string(kIndividualEarly), std::string(kClassDeadline)),
              kClassDeadline);
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::string(kIndividualLate), std::string(kClassDeadline)),
              kIndividualLate);
    // Equal instants: either string is correct; the original is kept.
    EXPECT_EQ(DeadlineEvaluator::applicableDeadline(orig, std::string(orig), std::string(orig)), orig);
    EXPECT_THROW(DeadlineEvaluator::applicableDeadline(orig, std::string("garbage"), std::nullopt),
                 std::invalid_argument);
}
