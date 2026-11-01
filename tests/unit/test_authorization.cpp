// T-AUTHZ / T-SCOPE: role x operation negative matrix (NFR-05: 100% of prohibited actions denied),
// forged and stale sessions, and resource-scope checks (FR-02, FR-08, FR-12, FR-15, FR-18).
#include "TestSupport.hpp"

#include <functional>
#include <vector>

using namespace lmstest;

namespace {
struct Operation {
    const char* name;
    Role allowed;
    std::function<void(LmsFixture&, const Session&)> call;
};
} // namespace

class Authz : public LmsFixture {
  public:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
        assignment_ = newAssignment();
        submission_ = submitAt(kBeforeDeadline, assignment_, student_);
        mark_ = svc().markSubmission(lecturer_, submission_, 50.0, "fb", "initial");
        file_ = makeFile("work.txt", "text");
    }
    int assignment_ = 0, submission_ = 0, mark_ = 0;
    fs::path file_;
};

TEST_F(Authz, T_AUTHZ_01_every_prohibited_role_and_operation_pair_is_denied) {
    const std::vector<Operation> ops = {
        {"createDepartment", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().createDepartment(s, "D2"); }},
        {"createUser", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().createUser(s, "x1", "LongPassword12", "X", Role::Student, 0); }},
        {"createCourse", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().createCourse(s, f.dep_, "C1", "T"); }},
        {"assignLecturer", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().assignLecturer(s, f.course_, f.lecturerId_); }},
        {"enrolStudent", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().enrolStudent(s, f.course_, f.outsiderId_); }},
        {"deactivateUser", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().deactivateUser(s, f.outsiderId_); }},
        {"updateUserAccount", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().updateUserAccount(s, f.outsiderId_, "N", Role::Student, 0); }},
        {"updateDepartment", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().updateDepartment(s, f.dep_, "Sci2"); }},
        {"updateCourse", Role::Administrator,
         [](LmsFixture& f, const Session& s) { f.svc().updateCourse(s, f.course_, "SCI101", "T", f.dep_); }},
        {"createAssignment", Role::Lecturer,
         [](LmsFixture& f, const Session& s) { f.svc().createAssignment(s, f.course_, "T", "d", kDeadline, 10); }},
        {"grantIndividualExtension", Role::Lecturer,
         [](LmsFixture& f, const Session& s) {
             f.svc().grantIndividualExtension(s, static_cast<Authz&>(f).assignment_, f.studentId_,
                                              "2026-10-30T00:00:00Z", "reason");
         }},
        {"submissionsForMarking", Role::Lecturer,
         [](LmsFixture& f, const Session& s) { f.svc().submissionsForMarking(s); }},
        {"markSubmission", Role::Lecturer,
         [](LmsFixture& f, const Session& s) {
             f.svc().markSubmission(s, static_cast<Authz&>(f).submission_, 50.0, "fb", "reason");
         }},
        {"moderate", Role::HeadOfDepartment,
         [](LmsFixture& f, const Session& s) { f.svc().moderate(s, static_cast<Authz&>(f).mark_, true, "reason"); }},
        {"release", Role::HeadOfDepartment,
         [](LmsFixture& f, const Session& s) { f.svc().release(s, static_cast<Authz&>(f).mark_); }},
        {"submit", Role::Student,
         [](LmsFixture& f, const Session& s) { f.svc().submit(s, static_cast<Authz&>(f).assignment_, "ref"); }},
        {"submitFile", Role::Student,
         [](LmsFixture& f, const Session& s) {
             f.svc().submitFile(s, static_cast<Authz&>(f).assignment_, static_cast<Authz&>(f).file_, f.dir_ / "store");
         }},
        {"releasedResults", Role::Student, [](LmsFixture& f, const Session& s) { f.svc().releasedResults(s); }},
    };
    const std::vector<std::pair<Role, const Session*>> actors = {{Role::Administrator, &admin_},
                                                                 {Role::Lecturer, &lecturer_},
                                                                 {Role::HeadOfDepartment, &hod_},
                                                                 {Role::Student, &student_}};
    int denied = 0, tried = 0;
    for (const auto& op : ops) {
        for (const auto& [role, session] : actors) {
            if (role == op.allowed)
                continue;
            ++tried;
            try {
                op.call(*this, *session);
                ADD_FAILURE() << lms::domain::roleName(role) << " was allowed to call " << op.name;
            } catch (const std::exception& e) {
                if (std::string(e.what()).find("Access denied") != std::string::npos)
                    ++denied;
                else
                    ADD_FAILURE() << op.name << " as " << lms::domain::roleName(role)
                                  << " failed for another reason: " << e.what();
            }
        }
    }
    EXPECT_EQ(tried, 18 * 3);
    EXPECT_EQ(denied, tried) << "NFR-05: every prohibited action must be denied";
}

TEST_F(Authz, T_AUTHZ_02_an_allowed_role_is_not_denied) {
    EXPECT_NO_THROW(svc().createDepartment(admin_, "History"));
    EXPECT_NO_THROW(svc().submissionsForMarking(lecturer_));
    EXPECT_NO_THROW(svc().releasedResults(student_));
    EXPECT_NO_THROW(svc().moderate(hod_, mark_, true, "ok"));
}

TEST_F(Authz, T_AUTHZ_03_forged_role_in_a_session_is_rejected) {
    Session forged = student_;
    forged.role = Role::Administrator;
    EXPECT_FAILS_WITH(svc().createDepartment(forged, "Injected"), "Access denied");
    forged = student_;
    forged.role = Role::HeadOfDepartment;
    EXPECT_FAILS_WITH(svc().release(forged, mark_), "Access denied");
}

TEST_F(Authz, T_AUTHZ_04_forged_user_id_or_department_is_rejected) {
    Session forged = admin_;
    forged.userId = 0;
    EXPECT_FAILS_WITH(svc().createDepartment(forged, "X"), "Access denied");
    forged.userId = 99999;
    EXPECT_FAILS_WITH(svc().createDepartment(forged, "X"), "Access denied");
    Session hodForged = hod_;
    hodForged.departmentId = otherDep_; // claim to head another department
    EXPECT_FAILS_WITH(svc().moderate(hodForged, mark_, true, "x"), "Access denied");
}

TEST_F(Authz, T_AUTHZ_05_session_of_a_deactivated_account_stops_working) {
    svc().deactivateUser(admin_, lecturerId_);
    EXPECT_FAILS_WITH(svc().submissionsForMarking(lecturer_), "Access denied");
    EXPECT_FAILS_WITH(svc().listAssignments(lecturer_), "Access denied");
}

TEST_F(Authz, T_AUTHZ_06_listing_needs_a_valid_session) {
    Session none;
    EXPECT_FAILS_WITH(svc().listAssignments(none), "Access denied");
    Session wrongRole = student_;
    wrongRole.role = Role::Lecturer;
    EXPECT_FAILS_WITH(svc().listAssignments(wrongRole), "Access denied");
}

// ---- T-SCOPE: resource scope ----

TEST_F(Authz, T_SCOPE_01_lecturer_cannot_create_assignments_for_an_unassigned_course) {
    EXPECT_FAILS_WITH(svc().createAssignment(lecturer_, otherCourse_, "T", "d", kDeadline, 10), "not assigned");
}

TEST_F(Authz, T_SCOPE_02_lecturer_cannot_mark_or_extend_in_another_course) {
    EXPECT_FAILS_WITH(svc().markSubmission(otherLecturer_, submission_, 50, "fb", "r"), "not assigned");
    EXPECT_FAILS_WITH(
        svc().grantIndividualExtension(otherLecturer_, assignment_, studentId_, "2026-10-30T00:00:00Z", "r"),
        "not assigned");
}

TEST_F(Authz, T_SCOPE_03_lecturer_queue_only_shows_own_courses) {
    EXPECT_TRUE(svc().submissionsForMarking(otherLecturer_).empty());
    EXPECT_EQ(svc().markSubmission(lecturer_, submission_, 10, "fb", "r"), mark_);
}

TEST_F(Authz, T_SCOPE_04_student_must_be_enrolled_to_submit) {
    EXPECT_FAILS_WITH(svc().submit(outsider_, assignment_, "ref"), "not enrolled");
    EXPECT_EQ(scalar("SELECT count(*) FROM submissions WHERE student_id=" + std::to_string(outsiderId_)), 0);
}

TEST_F(Authz, T_SCOPE_05_student_sees_only_assignments_of_enrolled_courses) {
    svc().createAssignment(otherLecturer_, otherCourse_, "Art essay", "d", kDeadline, 10);
    EXPECT_EQ(svc().listAssignments(student_).size(), 1U); // SCI101 only
    EXPECT_TRUE(svc().listAssignments(outsider_).empty()); // enrolled in no course
}

TEST_F(Authz, T_SCOPE_06_role_scoped_assignment_listings) {
    svc().createAssignment(otherLecturer_, otherCourse_, "Art essay", "d", kDeadline, 10);
    EXPECT_EQ(svc().listAssignments(lecturer_).size(), 1U);
    EXPECT_EQ(svc().listAssignments(otherLecturer_).size(), 1U);
    EXPECT_EQ(svc().listAssignments(hod_).size(), 1U);      // Science department only
    EXPECT_EQ(svc().listAssignments(otherHod_).size(), 1U); // Arts department only
    EXPECT_EQ(svc().listAssignments(admin_).size(), 2U);    // administrator sees all
}

TEST_F(Authz, T_SCOPE_07_hod_cannot_moderate_or_release_another_departments_mark) {
    EXPECT_FAILS_WITH(svc().moderate(otherHod_, mark_, true, "r"), "outside HoD department");
    svc().moderate(hod_, mark_, true, "ok");
    EXPECT_FAILS_WITH(svc().release(otherHod_, mark_), "outside HoD department");
    EXPECT_EQ(scalar("SELECT count(*) FROM release_records"), 0);
}

TEST_F(Authz, T_SCOPE_08_student_cannot_see_another_students_released_result) {
    const int sub2 = submitAt(kBeforeDeadline, assignment_, student2_);
    const int mark2 = svc().markSubmission(lecturer_, sub2, 70, "fb2", "r");
    svc().moderate(hod_, mark2, true, "ok");
    svc().release(hod_, mark2);
    EXPECT_EQ(svc().releasedResults(student2_).size(), 1U);
    EXPECT_TRUE(svc().releasedResults(student_).empty());
}
