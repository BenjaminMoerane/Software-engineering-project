// T-ADMIN: account, department, course, lecturer-assignment and enrolment management (FR-03, FR-04, FR-05).
#include "TestSupport.hpp"

using namespace lmstest;

class Admin : public LmsFixture {
  protected:
    void SetUp() override {
        LmsFixture::SetUp();
        buildWorld();
    }
};

TEST_F(Admin, T_ADMIN_01_create_users_of_each_role) {
    EXPECT_EQ(scalar("SELECT count(*) FROM users WHERE role='Student'"), 3);
    EXPECT_EQ(scalar("SELECT count(*) FROM users WHERE role='Lecturer'"), 2);
    EXPECT_EQ(scalar("SELECT count(*) FROM users WHERE role='HeadOfDepartment'"), 2);
    EXPECT_EQ(scalar("SELECT count(*) FROM users WHERE role='Administrator'"), 1);
}

TEST_F(Admin, T_ADMIN_02_user_input_validation) {
    EXPECT_FAILS_WITH(svc().createUser(admin_, "", "LongPassword12", "N", Role::Student, 0),
                      "Username must not be empty");
    EXPECT_FAILS_WITH(svc().createUser(admin_, "u", "short", "N", Role::Student, 0), "at least 10 characters");
    EXPECT_FAILS_WITH(svc().createUser(admin_, "u", "LongPassword12", "", Role::Student, 0),
                      "Full name must not be empty");
    EXPECT_FAILS_WITH(svc().createUser(admin_, "u", "LongPassword12", "N", Role::HeadOfDepartment, 0),
                      "HeadOfDepartment must belong to a department");
    EXPECT_FAILS_WITH(svc().createUser(admin_, "u", "LongPassword12", "N", Role::Student, -1), "Invalid department ID");
}

TEST_F(Admin, T_ADMIN_03_duplicate_username_is_rejected_without_a_partial_record) {
    const int before = scalar("SELECT count(*) FROM users");
    EXPECT_ANY_THROW(svc().createUser(admin_, "student", "LongPassword12", "Dup", Role::Student, dep_));
    EXPECT_EQ(scalar("SELECT count(*) FROM users"), before);
}

TEST_F(Admin, T_ADMIN_04_unknown_department_is_rejected_by_the_database) {
    EXPECT_ANY_THROW(svc().createUser(admin_, "ghost", "LongPassword12", "Ghost", Role::Student, 9999));
    EXPECT_ANY_THROW(svc().createCourse(admin_, 9999, "X1", "No department"));
}

TEST_F(Admin, T_ADMIN_05_deactivate_user_and_not_oneself) {
    svc().deactivateUser(admin_, studentId_);
    EXPECT_EQ(scalar("SELECT active FROM users WHERE user_id=" + std::to_string(studentId_)), 0);
    EXPECT_FAILS_WITH(svc().deactivateUser(admin_, studentId_), "Active user account not found"); // already inactive
    EXPECT_FAILS_WITH(svc().deactivateUser(admin_, 9999), "Active user account not found");
    EXPECT_FAILS_WITH(svc().deactivateUser(admin_, admin_.userId), "cannot deactivate the currently authenticated");
    EXPECT_EQ(scalar("SELECT active FROM users WHERE user_id=" + std::to_string(admin_.userId)), 1);
}

TEST_F(Admin, T_ADMIN_06_update_account_name_role_department) {
    svc().updateUserAccount(admin_, studentId_, "Renamed Student", Role::Student, dep_);
    EXPECT_EQ(scalarText("SELECT full_name FROM users WHERE user_id=" + std::to_string(studentId_)), "Renamed Student");
    svc().updateUserAccount(admin_, studentId_, "Now Lecturer", Role::Lecturer, 0);
    EXPECT_EQ(scalarText("SELECT role FROM users WHERE user_id=" + std::to_string(studentId_)), "Lecturer");
    EXPECT_EQ(scalar("SELECT department_id IS NULL FROM users WHERE user_id=" + std::to_string(studentId_)), 1);
    EXPECT_FAILS_WITH(svc().updateUserAccount(admin_, studentId_, "", Role::Student, 0), "Full name must not be empty");
    EXPECT_FAILS_WITH(svc().updateUserAccount(admin_, studentId_, "N", Role::HeadOfDepartment, 0),
                      "HeadOfDepartment must belong to a department");
    EXPECT_FAILS_WITH(svc().updateUserAccount(admin_, studentId_, "N", Role::Student, -2), "Invalid department ID");
    EXPECT_FAILS_WITH(svc().updateUserAccount(admin_, 9999, "N", Role::Student, 0), "User account not found");
}

TEST_F(Admin, T_ADMIN_07_departments_create_update_list) {
    const int id = svc().createDepartment(admin_, "History");
    svc().updateDepartment(admin_, id, "Modern History");
    const auto names = svc().listDepartments();
    EXPECT_EQ(names.size(), 3U);
    bool found = false;
    for (const auto& n : names)
        found = found || n.find("Modern History") != std::string::npos;
    EXPECT_TRUE(found);
    EXPECT_FAILS_WITH(svc().createDepartment(admin_, "  "), "Department name must not be empty");
    EXPECT_ANY_THROW(svc().createDepartment(admin_, "Modern History")); // unique
    EXPECT_FAILS_WITH(svc().updateDepartment(admin_, 9999, "X"), "Department not found");
    EXPECT_FAILS_WITH(svc().updateDepartment(admin_, id, ""), "Department name must not be empty");
}

TEST_F(Admin, T_ADMIN_08_courses_create_update) {
    const int id = svc().createCourse(admin_, dep_, "SCI202", "Physics");
    svc().updateCourse(admin_, id, "SCI203", "Advanced Physics", otherDep_);
    EXPECT_EQ(scalarText("SELECT course_code FROM courses WHERE course_id=" + std::to_string(id)), "SCI203");
    EXPECT_EQ(scalar("SELECT department_id FROM courses WHERE course_id=" + std::to_string(id)), otherDep_);
    EXPECT_FAILS_WITH(svc().createCourse(admin_, dep_, "", "T"), "Course code must not be empty");
    EXPECT_FAILS_WITH(svc().createCourse(admin_, dep_, "C", ""), "Course title must not be empty");
    EXPECT_ANY_THROW(svc().createCourse(admin_, dep_, "SCI101", "Duplicate code"));
    EXPECT_FAILS_WITH(svc().updateCourse(admin_, 9999, "A", "B", dep_), "Course not found");
    EXPECT_FAILS_WITH(svc().updateCourse(admin_, id, "", "B", dep_), "Course code must not be empty");
    EXPECT_FAILS_WITH(svc().updateCourse(admin_, id, "A", "", dep_), "Course title must not be empty");
}

TEST_F(Admin, T_ADMIN_09_assign_lecturer_rules) {
    EXPECT_FAILS_WITH(svc().assignLecturer(admin_, course_, studentId_), "must be an active Lecturer");
    EXPECT_FAILS_WITH(svc().assignLecturer(admin_, course_, 9999), "must be an active Lecturer");
    svc().deactivateUser(admin_, otherLecturerId_);
    EXPECT_FAILS_WITH(svc().assignLecturer(admin_, course_, otherLecturerId_), "must be an active Lecturer");
    EXPECT_ANY_THROW(svc().assignLecturer(admin_, course_, lecturerId_)); // already assigned
    EXPECT_ANY_THROW(svc().assignLecturer(admin_, 9999, lecturerId_));    // no such course
}

TEST_F(Admin, T_ADMIN_10_enrol_student_rules) {
    EXPECT_FAILS_WITH(svc().enrolStudent(admin_, course_, lecturerId_), "must be an active Student");
    EXPECT_FAILS_WITH(svc().enrolStudent(admin_, course_, 9999), "must be an active Student");
    EXPECT_ANY_THROW(svc().enrolStudent(admin_, course_, studentId_)); // already enrolled
    EXPECT_EQ(scalar("SELECT count(*) FROM enrolments WHERE course_id=" + std::to_string(course_)), 2);
    EXPECT_ANY_THROW(svc().enrolStudent(admin_, 9999, outsiderId_)); // no such course
    svc().deactivateUser(admin_, outsiderId_);
    EXPECT_FAILS_WITH(svc().enrolStudent(admin_, course_, outsiderId_), "must be an active Student");
}
