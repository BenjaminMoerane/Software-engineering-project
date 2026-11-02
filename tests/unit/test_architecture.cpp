// Architecture tests (T-ARCH-*): verify the Phase 3 design patterns are real in the code, not only in the diagram.
//  * Repository/DAO + Dependency Inversion: services run against in-memory fakes with no SQLite at all.
//  * Strategy: MarkingService uses whatever IPenaltyStrategy it is given (Open/Closed).
//  * Unit of work: a failed audit write leaves the transaction uncommitted (FR-14 / NFR-06 atomicity).
//  * DeadlineEvaluator: boundary-value analysis of the lateness calculation in pure C++.
#include "services/DeadlineEvaluator.hpp"
#include "services/PenaltyStrategy.hpp"
#include "services/Services.hpp"

#include <gtest/gtest.h>

#include <map>
#include <stdexcept>

namespace {
using namespace lms::services;
using namespace lms::domain;
namespace repo = lms::repositories;

// ------------------------------------------------------------------ in-memory fakes
struct FakeTx : repo::ITransactionManager {
    int begun = 0, committed = 0;
    struct T : repo::ITransaction {
        FakeTx& owner;
        explicit T(FakeTx& o) : owner(o) {}
        void commit() override { ++owner.committed; }
    };
    std::unique_ptr<repo::ITransaction> begin() override {
        ++begun;
        return std::make_unique<T>(*this);
    }
};

struct FakeUsers : repo::IUserRepository {
    std::map<int, AccountState> accounts;
    int countUsers() const override { return static_cast<int>(accounts.size()); }
    std::optional<UserCredentials> findCredentials(const std::string&) const override { return std::nullopt; }
    std::optional<AccountState> findAccountState(int id) const override {
        auto it = accounts.find(id);
        return it == accounts.end() ? std::nullopt : std::optional<AccountState>(it->second);
    }
    int insert(const NewUser&) override { return 0; }
    bool deactivate(int) override { return false; }
    bool update(int, const std::string&, const std::string&, int) override { return false; }
};

struct FakeCourses : repo::ICourseRepository {
    std::map<int, AssignmentInfo> assignments;
    std::map<std::pair<int, int>, std::string> extensions; // (assignment, student) -> deadline
    bool lecturerAssigned = true, studentEnrolled = true;
    int insertDepartment(const std::string&) override { return 0; }
    bool updateDepartment(int, const std::string&) override { return false; }
    std::vector<std::pair<int, std::string>> listDepartments() const override { return {}; }
    int insertCourse(int, const std::string&, const std::string&) override { return 0; }
    bool updateCourse(int, const std::string&, const std::string&, int) override { return false; }
    std::optional<int> departmentOfCourse(int) const override { return 1; }
    void assignLecturer(int, int) override {}
    void enrolStudent(int, int) override {}
    bool isLecturerAssigned(int, int) const override { return lecturerAssigned; }
    bool isStudentEnrolled(int, int) const override { return studentEnrolled; }
    int insertAssignment(const NewAssignment&) override { return 0; }
    std::optional<AssignmentInfo> findActiveAssignment(int id) const override {
        auto it = assignments.find(id);
        return it == assignments.end() ? std::nullopt : std::optional<AssignmentInfo>(it->second);
    }
    std::vector<AssignmentView> listAssignmentsVisibleTo(int, const std::string&, int) const override { return {}; }
    void upsertExtension(int a, int s, int, const std::string& d, const std::string&) override {
        extensions[{a, s}] = d;
    }
    std::optional<std::string> findExtensionDeadline(int a, int s) const override {
        auto it = extensions.find({a, s});
        return it == extensions.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
};

struct FakeSubmissions : repo::ISubmissionRepository {
    std::vector<NewSubmissionVersion> saved;
    std::optional<ExistingSubmission> existing;
    MarkingContext context{1, 100.0, 7, 0, "OnTime"};
    std::optional<ExistingSubmission> find(int, int) const override { return existing; }
    int saveVersion(const NewSubmissionVersion& v) override {
        saved.push_back(v);
        return v.submissionId == 0 ? 42 : v.submissionId;
    }
    std::optional<MarkingContext> findMarkingContext(int) const override { return context; }
    std::vector<SubmissionView> markingQueueFor(int) const override { return {}; }
};

struct FakeMarks : repo::IMarkRepository {
    std::vector<MarkWrite> inserted;
    std::vector<MarkAuditEntry> audits;
    bool failAudit = false;
    std::optional<CurrentMark> findCurrent(int) const override { return std::nullopt; }
    void updateCurrent(int, const MarkWrite&) override {}
    int insertCurrent(const MarkWrite& m) override {
        inserted.push_back(m);
        return 9;
    }
    void appendAudit(const MarkAuditEntry& e) override {
        if (failAudit)
            throw std::runtime_error("injected audit failure");
        audits.push_back(e);
    }
};

struct FakeModeration : repo::IModerationRepository {
    std::optional<int> departmentOfCurrentMark(int) const override { return 1; }
    std::optional<std::string> latestDecision(int) const override { return std::nullopt; }
    bool isReleased(int) const override { return false; }
    void insertDecision(int, int, const std::string&, const std::string&) override {}
    void insertRelease(int, int) override {}
    std::vector<ModerationQueueRow> unreleasedCurrentMarksFor(int) const override { return {}; }
    std::vector<ResultView> releasedResultsFor(int) const override { return {}; }
};

// A different late rule, to prove MarkingService is closed for modification but open for extension.
struct FlatFivePenalty : IPenaltyStrategy {
    PenaltyResult calculate(int days, double) const override { return {days > 0 ? 5.0 : 0.0, false}; }
};

class ArchitectureTest : public ::testing::Test {
  protected:
    FakeTx tx;
    FakeUsers users;
    FakeCourses courses;
    FakeSubmissions submissions;
    FakeMarks marks;
    FakeModeration moderation;
    Session student{10, "stu", "Student One", Role::Student, 0};
    Session lecturer{20, "lec", "Lecturer One", Role::Lecturer, 0};
    void SetUp() override {
        users.accounts[10] = {true, "Student", 0};
        users.accounts[20] = {true, "Lecturer", 0};
        courses.assignments[1] = {1, "2026-10-20T12:00:00Z", 100.0};
    }
};

// ------------------------------------------------------------------ Repository / DIP
TEST_F(ArchitectureTest, T_ARCH_01_submission_service_runs_without_sqlite_and_records_timing_facts) {
    AuthorizationService authz(users, courses);
    SubmissionService svc(courses, submissions, tx, authz, [] { return std::string("2026-10-21T12:00:01Z"); });
    EXPECT_EQ(svc.submit(student, 1, "essay.pdf"), 42);
    ASSERT_EQ(submissions.saved.size(), 1u);
    const auto& v = submissions.saved.front();
    EXPECT_EQ(v.versionNo, 1);
    EXPECT_EQ(v.latenessDays, 2); // 24h + 1s late -> 2 elapsed 24-hour periods (rounded up)
    EXPECT_EQ(v.lateStatus, "LateWithinWindow");
    EXPECT_EQ(v.deadlineUsed, "2026-10-20T12:00:00Z");
    EXPECT_EQ(tx.committed, 1);
}

TEST_F(ArchitectureTest, T_ARCH_02_extension_deadline_is_the_applicable_deadline) {
    courses.extensions[{1, 10}] = "2026-10-25T12:00:00Z";
    AuthorizationService authz(users, courses);
    SubmissionService svc(courses, submissions, tx, authz, [] { return std::string("2026-10-21T12:00:01Z"); });
    svc.submit(student, 1, "essay.pdf");
    EXPECT_EQ(submissions.saved.front().latenessDays, 0);
    EXPECT_EQ(submissions.saved.front().deadlineUsed, "2026-10-25T12:00:00Z");
}

TEST_F(ArchitectureTest, T_ARCH_03_late_resubmission_is_rejected_before_any_write) {
    submissions.existing = ExistingSubmission{42, 1};
    AuthorizationService authz(users, courses);
    SubmissionService svc(courses, submissions, tx, authz, [] { return std::string("2026-10-20T12:00:01Z"); });
    EXPECT_THROW(svc.submit(student, 1, "v2.pdf"), std::runtime_error);
    EXPECT_TRUE(submissions.saved.empty());
    EXPECT_EQ(tx.begun, 0);
}

TEST_F(ArchitectureTest, T_ARCH_04_authorization_rechecks_stored_account_state) {
    users.accounts[10].active = false;
    AuthorizationService authz(users, courses);
    EXPECT_THROW(authz.requireRole(student, Role::Student), std::runtime_error);
    users.accounts[10] = {true, "Lecturer", 0}; // role changed in storage after login
    EXPECT_THROW(authz.requireRole(student, Role::Student), std::runtime_error);
}

// ------------------------------------------------------------------ Strategy
TEST_F(ArchitectureTest, T_ARCH_05_marking_uses_the_injected_penalty_strategy) {
    submissions.context.latenessDays = 2;
    AuthorizationService authz(users, courses);
    AuditService audit(marks);
    FlatFivePenalty flat;
    MarkingService svc(submissions, marks, moderation, tx, authz, audit, flat);
    svc.markSubmission(lecturer, 1, 80.0, "Good", "first mark");
    ASSERT_EQ(marks.inserted.size(), 1u);
    EXPECT_DOUBLE_EQ(marks.inserted.front().penaltyAmount, 5.0);
    EXPECT_DOUBLE_EQ(marks.inserted.front().finalMark, 75.0);
}

TEST_F(ArchitectureTest, T_ARCH_06_provisional_strategy_applies_ten_percent_per_day) {
    submissions.context.latenessDays = 2;
    AuthorizationService authz(users, courses);
    AuditService audit(marks);
    ProvisionalLinearPenaltyStrategy provisional;
    MarkingService svc(submissions, marks, moderation, tx, authz, audit, provisional);
    svc.markSubmission(lecturer, 1, 80.0, "Good", "first mark");
    EXPECT_DOUBLE_EQ(marks.inserted.front().rawMark, 80.0);
    EXPECT_DOUBLE_EQ(marks.inserted.front().penaltyAmount, 20.0);
    EXPECT_DOUBLE_EQ(marks.inserted.front().finalMark, 60.0);
    ASSERT_EQ(marks.audits.size(), 1u);
    EXPECT_FALSE(marks.audits.front().oldMark.has_value());
    EXPECT_DOUBLE_EQ(marks.audits.front().newMark, 60.0);
}

TEST(PenaltyStrategy, T_ARCH_07_boundaries_of_the_provisional_rule) {
    ProvisionalLinearPenaltyStrategy s;
    EXPECT_DOUBLE_EQ(s.calculate(0, 50).penalty, 0.0);
    EXPECT_DOUBLE_EQ(s.calculate(1, 50).penalty, 5.0);
    EXPECT_DOUBLE_EQ(s.calculate(3, 50).penalty, 15.0);
    EXPECT_FALSE(s.calculate(3, 50).zeroed);
    EXPECT_TRUE(s.calculate(4, 50).zeroed);
    EXPECT_DOUBLE_EQ(applyPenalty(10.0, s.calculate(3, 50)), 0.0); // clamped at zero, never negative
    EXPECT_THROW(s.calculate(-1, 50), std::invalid_argument);
    EXPECT_THROW(s.calculate(1, 0), std::invalid_argument);
}

// ------------------------------------------------------------------ Unit of work / audit atomicity
TEST_F(ArchitectureTest, T_ARCH_08_audit_failure_leaves_transaction_uncommitted) {
    marks.failAudit = true;
    AuthorizationService authz(users, courses);
    AuditService audit(marks);
    ProvisionalLinearPenaltyStrategy provisional;
    MarkingService svc(submissions, marks, moderation, tx, authz, audit, provisional);
    EXPECT_THROW(svc.markSubmission(lecturer, 1, 70.0, "ok", "first mark"), std::runtime_error);
    EXPECT_EQ(tx.begun, 1);
    EXPECT_EQ(tx.committed, 0); // the SQLite implementation rolls back on destruction
}

TEST_F(ArchitectureTest, T_ARCH_09_audit_service_requires_a_reason) {
    AuditService audit(marks);
    EXPECT_THROW(audit.recordMarkChange(1, 20, std::nullopt, 50.0, "   "), std::invalid_argument);
    EXPECT_TRUE(marks.audits.empty());
}

// ------------------------------------------------------------------ DeadlineEvaluator (BVA)
TEST(DeadlineEvaluator, T_ARCH_10_lateness_boundaries) {
    const std::string d = "2026-10-20T12:00:00Z";
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-19T12:00:00Z", d), 0);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-20T12:00:00Z", d), 0); // exactly on deadline = on time
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-20T12:00:01Z", d), 1);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-21T12:00:00Z", d), 1);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-21T12:00:01Z", d), 2);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-23T12:00:00Z", d), 3);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2026-10-23T12:00:01Z", d), 4);
}

TEST(DeadlineEvaluator, T_ARCH_11_classification_partitions) {
    EXPECT_STREQ(DeadlineEvaluator::classify(0), "OnTime");
    EXPECT_STREQ(DeadlineEvaluator::classify(1), "LateWithinWindow");
    EXPECT_STREQ(DeadlineEvaluator::classify(3), "LateWithinWindow");
    EXPECT_STREQ(DeadlineEvaluator::classify(4), "BeyondWindow");
}

TEST(DeadlineEvaluator, T_ARCH_12_calendar_edges_month_year_and_leap_day) {
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2027-01-01T00:00:00Z", "2026-12-31T00:00:00Z"), 1);
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2028-03-01T00:00:00Z", "2028-02-28T00:00:00Z"), 2); // 2028 is leap
    EXPECT_EQ(DeadlineEvaluator::latenessDays("2027-03-01T00:00:00Z", "2027-02-28T00:00:00Z"), 1);
    EXPECT_EQ(DeadlineEvaluator::toEpochSeconds("1970-01-01T00:00:00Z"), 0);
    EXPECT_EQ(DeadlineEvaluator::toEpochSeconds("2026-10-20T12:00:00.999Z"),
              DeadlineEvaluator::toEpochSeconds("2026-10-20T12:00:00Z")); // fractional seconds truncated
}

TEST(DeadlineEvaluator, T_ARCH_13_malformed_timestamps_are_rejected_not_treated_as_on_time) {
    EXPECT_THROW(DeadlineEvaluator::latenessDays("not a date", "2026-10-20T12:00:00Z"), std::invalid_argument);
    EXPECT_THROW(DeadlineEvaluator::latenessDays("2026-10-20T12:00:00Z", "2026-13-01T00:00:00Z"),
                 std::invalid_argument);
    EXPECT_THROW(DeadlineEvaluator::toEpochSeconds("2026-10-20T12:00:00+02:00"), std::invalid_argument);
}

TEST_F(ArchitectureTest, T_ARCH_14_unenrolled_student_cannot_submit_even_with_fakes) {
    courses.studentEnrolled = false;
    AuthorizationService authz(users, courses);
    SubmissionService svc(courses, submissions, tx, authz, [] { return std::string("2026-10-19T12:00:00Z"); });
    EXPECT_THROW(svc.submit(student, 1, "essay.pdf"), std::runtime_error);
    EXPECT_TRUE(submissions.saved.empty());
}

} // namespace
