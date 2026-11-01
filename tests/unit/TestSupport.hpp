// Shared fixture for the GoogleTest suite. Each test gets its own temporary directory and SQLite database,
// a controllable clock and a ready-made "school" (departments, users of every role, a course, enrolments).
#pragma once

#include "data/Database.hpp"
#include "domain/User.hpp"
#include "services/LmsService.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace lmstest {

namespace fs = std::filesystem;
using lms::domain::Role;
using lms::domain::Session;
using lms::services::LmsService;

inline const char* const kAdminPassword = "AdminPassw0rd!!";
inline const char* const kUserPassword = "CorrectHorse!Battery9";
// Default assignment deadline, and a "now" that is comfortably before it.
inline const char* const kDeadline = "2026-10-20T12:00:00Z";
inline const char* const kBeforeDeadline = "2026-10-19T12:00:00Z";

// Expect `stmt` to throw a std::exception whose message contains `text`.
#define EXPECT_FAILS_WITH(stmt, text)                                                                                  \
    do {                                                                                                               \
        try {                                                                                                          \
            stmt;                                                                                                      \
            ADD_FAILURE() << "expected an exception containing: " << (text);                                           \
        } catch (const std::exception& lmsError) {                                                                     \
            EXPECT_NE(std::string(lmsError.what()).find(text), std::string::npos)                                      \
                << "actual message: " << lmsError.what();                                                              \
        }                                                                                                              \
    } while (0)

class LmsFixture : public ::testing::Test {
  public: // test helper: members are public so table-driven tests can use them in lambdas
    void SetUp() override {
        static int counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir_ = fs::temp_directory_path() / ("lms_gtest_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        fs::create_directories(dir_);
        dbPath_ = (dir_ / "lms.sqlite3").string();
        openDatabase();
    }

    void TearDown() override {
        svc_.reset();
        db_.reset();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    void openDatabase() {
        db_ = std::make_unique<lms::data::Database>(dbPath_);
        db_->initialiseSchema();
        svc_ = std::make_unique<LmsService>(*db_, [this] { return now_; });
    }

    // Close and reopen the database file, as an application restart would.
    void reopen() {
        svc_.reset();
        db_.reset();
        openDatabase();
    }

    LmsService& svc() { return *svc_; }
    lms::data::Database& db() { return *db_; }

    // Administrator, two departments, one HoD per department, lecturers, students and a course.
    void buildWorld() {
        svc().bootstrapAdministrator("admin", kAdminPassword, "System Administrator");
        admin_ = svc().login("admin", kAdminPassword);
        dep_ = svc().createDepartment(admin_, "Science");
        otherDep_ = svc().createDepartment(admin_, "Arts");
        hodId_ = addUser("hod", Role::HeadOfDepartment, dep_);
        otherHodId_ = addUser("otherhod", Role::HeadOfDepartment, otherDep_);
        lecturerId_ = addUser("lecturer", Role::Lecturer, dep_);
        otherLecturerId_ = addUser("otherlecturer", Role::Lecturer, otherDep_);
        studentId_ = addUser("student", Role::Student, dep_);
        student2Id_ = addUser("student2", Role::Student, dep_);
        outsiderId_ = addUser("outsider", Role::Student, dep_);
        hod_ = svc().login("hod", kUserPassword);
        otherHod_ = svc().login("otherhod", kUserPassword);
        lecturer_ = svc().login("lecturer", kUserPassword);
        otherLecturer_ = svc().login("otherlecturer", kUserPassword);
        student_ = svc().login("student", kUserPassword);
        student2_ = svc().login("student2", kUserPassword);
        outsider_ = svc().login("outsider", kUserPassword);
        course_ = svc().createCourse(admin_, dep_, "SCI101", "Science");
        otherCourse_ = svc().createCourse(admin_, otherDep_, "ART100", "Arts");
        svc().assignLecturer(admin_, course_, lecturerId_);
        svc().assignLecturer(admin_, otherCourse_, otherLecturerId_);
        svc().enrolStudent(admin_, course_, studentId_);
        svc().enrolStudent(admin_, course_, student2Id_);
        now_ = kBeforeDeadline;
    }

    int addUser(const std::string& username, Role role, int departmentId) {
        return svc().createUser(admin_, username, kUserPassword, username + " Person", role, departmentId);
    }

    int newAssignment(const std::string& deadline = kDeadline, double maxMark = 100.0,
                      const std::string& title = "Lab report") {
        return svc().createAssignment(lecturer_, course_, title, "description", deadline, maxMark);
    }

    // Submit at a given (fake) time and return the submission id.
    int submitAt(const std::string& when, int assignmentId, const Session& who) {
        now_ = when;
        return svc().submit(who, assignmentId, "content-ref");
    }

    int scalar(const std::string& sql) {
        lms::data::Statement q(db_->handle(), sql);
        return q.step() ? q.integer(0) : -1;
    }
    double scalarReal(const std::string& sql) {
        lms::data::Statement q(db_->handle(), sql);
        return q.step() ? q.real(0) : -1.0;
    }
    std::string scalarText(const std::string& sql) {
        lms::data::Statement q(db_->handle(), sql);
        return q.step() ? q.text(0) : std::string();
    }

    int latenessOf(int submissionId) {
        return scalar("SELECT lateness_days FROM submission_versions WHERE is_current=1 AND submission_id=" +
                      std::to_string(submissionId));
    }
    std::string lateStatusOf(int submissionId) {
        return scalarText("SELECT late_status FROM submission_versions WHERE is_current=1 AND submission_id=" +
                          std::to_string(submissionId));
    }

    fs::path makeFile(const std::string& name, const std::string& content) {
        const fs::path p = dir_ / name;
        std::ofstream out(p, std::ios::binary);
        out << content;
        return p;
    }

    // Mark, approve and release one submission; returns the mark id.
    int markApproveRelease(int submissionId, double raw = 80.0) {
        const int markId = svc().markSubmission(lecturer_, submissionId, raw, "Feedback", "Initial mark");
        svc().moderate(hod_, markId, true, "Approved");
        svc().release(hod_, markId);
        return markId;
    }

    fs::path dir_;
    std::string dbPath_;
    std::string now_ = kBeforeDeadline;
    std::unique_ptr<lms::data::Database> db_;
    std::unique_ptr<LmsService> svc_;

    Session admin_, hod_, otherHod_, lecturer_, otherLecturer_, student_, student2_, outsider_;
    int dep_ = 0, otherDep_ = 0, course_ = 0, otherCourse_ = 0;
    int hodId_ = 0, otherHodId_ = 0, lecturerId_ = 0, otherLecturerId_ = 0;
    int studentId_ = 0, student2Id_ = 0, outsiderId_ = 0;
};

} // namespace lmstest
