#include "data/Database.hpp"
#include "services/LmsService.hpp"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>

namespace {
void require(bool ok, const std::string& message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void mustFail(F f, const std::string& message) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, message);
}
} // namespace
int main() {
    const std::string path = "lms_integration_test.sqlite3";
    std::remove(path.c_str());
    try {
        lms::data::Database db(path);
        db.initialiseSchema();
        std::string testNow = "2026-10-21T12:00:00Z";
        lms::services::LmsService svc(db, [&testNow] { return testNow; });
        int adminId = svc.bootstrapAdministrator("admin", "CorrectHorseBattery1!", "System Administrator");
        auto admin = svc.login("admin", "CorrectHorseBattery1!");
        require(admin.userId == adminId, "admin login should work");
        mustFail([&] { svc.login("admin", "wrong-password"); }, "wrong password must be rejected");
        int dep = svc.createDepartment(admin, "Science");
        int hodId = svc.createUser(admin, "hod", "CorrectHorseBattery2!", "Department Head",
                                   lms::domain::Role::HeadOfDepartment, dep);
        int lecturerId = svc.createUser(admin, "lecturer", "CorrectHorseBattery3!", "Lecturer One",
                                        lms::domain::Role::Lecturer, dep);
        int studentId =
            svc.createUser(admin, "student", "CorrectHorseBattery4!", "Student One", lms::domain::Role::Student, dep);
        int student2Id =
            svc.createUser(admin, "student2", "CorrectHorseBattery6!", "Student Two", lms::domain::Role::Student, dep);
        auto hod = svc.login("hod", "CorrectHorseBattery2!");
        auto lecturer = svc.login("lecturer", "CorrectHorseBattery3!");
        auto student = svc.login("student", "CorrectHorseBattery4!");
        auto student2 = svc.login("student2", "CorrectHorseBattery6!");
        require(hod.userId == hodId, "HoD login");
        int course = svc.createCourse(admin, dep, "SCI101", "Science");
        svc.assignLecturer(admin, course, lecturerId);
        svc.enrolStudent(admin, course, studentId);
        svc.enrolStudent(admin, course, student2Id);
        int assignment = svc.createAssignment(lecturer, course, "Lab report", "Report", "2026-10-20T12:00:00Z", 100.0);
        require(svc.listAssignments(student).size() == 1, "enrolled student sees course assignment");
        mustFail([&] { svc.createAssignment(student, course, "forbidden", "", "2026-10-21T12:00:00Z", 100); },
                 "student cannot create assignment");
        int otherCourse = svc.createCourse(admin, dep, "SCI202", "Physics");
        mustFail(
            [&] {
                svc.createAssignment(lecturer, otherCourse, "Unassigned course", "Denied", "2026-10-21T12:00:00Z", 100);
            },
            "lecturer cannot create assignment for an unassigned course");
        svc.assignLecturer(admin, otherCourse, lecturerId);
        int hiddenAssignment =
            svc.createAssignment(lecturer, otherCourse, "Physics lab", "Not enrolled", "2026-10-21T12:00:00Z", 100.0);
        (void)hiddenAssignment;
        require(svc.listAssignments(student2).size() == 1,
                "student sees only assignments for courses in which they are enrolled");
        mustFail([&] { svc.createDepartment(lecturer, "Forbidden department"); }, "lecturer cannot create department");
        mustFail([&] { svc.updateCourse(student, course, "SCI101", "Tampered title", dep); },
                 "student cannot update course");
        int submission = svc.submit(student, assignment, "submission-file-ref");
        require(submission > 0, "submission saved");
        testNow = "2026-02-31T12:00:00Z";
        mustFail([&] { svc.submit(student, assignment, "invalid-date-file"); },
                 "invalid calendar timestamp must be rejected");
        testNow = "2026-10-21T12:00:00Z";
        auto queue = svc.submissionsForMarking(lecturer);
        require(queue.size() == 1 && queue[0].lateStatus == "LateWithinWindow",
                "late submission enters lecturer queue");
        // Force the audit insert to fail and prove the mark/feedback mutation rolls back atomically.
        db.execute(
            "CREATE TRIGGER test_fail_audit BEFORE INSERT ON mark_audit BEGIN SELECT RAISE(ABORT,'forced audit failure'); END;");
        mustFail([&] { svc.markSubmission(lecturer, submission, 80.0, "Should roll back", "Forced failure"); },
                 "audit failure must fail the marking transaction");
        db.execute("DROP TRIGGER test_fail_audit;");
        lms::data::Statement noMark(db.handle(), "SELECT count(*) FROM marks WHERE submission_id=?1 AND current=1");
        noMark.bind(1, submission);
        require(noMark.step() && noMark.integer(0) == 0, "failed audit leaves no current mark behind");
        int mark = svc.markSubmission(lecturer, submission, 80.0, "Good structure", "Initial mark");
        mustFail([&] { svc.release(hod, mark); }, "unmoderated mark cannot be released");
        require(svc.releasedResults(student).empty(), "unreleased mark and feedback remain hidden");
        svc.moderate(hod, mark, false, "Please clarify the calculation");
        auto returned = svc.submissionsForMarking(lecturer);
        require(returned.size() == 1, "returned mark reappears in lecturer queue");
        int corrected =
            svc.markSubmission(lecturer, submission, 82.0, "Calculation clarified", "Corrected after moderation");
        require(corrected == mark, "returned mark is updated in place to preserve schema uniqueness");
        mustFail([&] { svc.release(hod, corrected); }, "corrected mark still needs moderation");
        svc.moderate(hod, corrected, true, "Reviewed and approved");
        svc.release(hod, corrected);
        auto results = svc.releasedResults(student);
        require(results.size() == 1, "released result visible to owner");
        require(results[0].mark == 72.0, "one late day deducts ten percent of available mark");
        require(results[0].feedback == "Calculation clarified", "latest feedback visible after release");
        require(svc.releasedResults(student2).empty(),
                "another student cannot see the first student's released result");
        mustFail([&] { svc.markSubmission(lecturer, submission, 90.0, "Changed after release", "Late correction"); },
                 "post-release correction blocked pending decision OD-08");
        mustFail([&] { svc.markSubmission(lecturer, submission, 100.01, "Out of range", "Invalid upper boundary"); },
                 "raw mark above maximum must be rejected");
        mustFail([&] { svc.markSubmission(lecturer, submission, -0.01, "Out of range", "Invalid lower boundary"); },
                 "negative raw mark must be rejected");
        // Exercise the individual-extension path and verify the student's displayed deadline is used.
        int extendedAssignment =
            svc.createAssignment(lecturer, course, "Extended lab", "Extension test", "2026-10-20T12:00:00Z", 50.0);
        svc.grantIndividualExtension(lecturer, extendedAssignment, student2Id, "2026-10-22T12:00:00Z",
                                     "Approved individual extension");
        auto student2Assignments = svc.listAssignments(student2);
        bool extensionVisible = false;
        for (const auto& item : student2Assignments) {
            if (item.id == extendedAssignment) {
                extensionVisible = (item.extensionDeadline == "2026-10-22T12:00:00Z");
            }
        }
        require(extensionVisible, "student sees their approved individual extension deadline");
        int extendedSubmission = svc.submit(student2, extendedAssignment, "extended-submission-ref");
        bool extensionSubmissionOnTime = false;
        for (const auto& item : svc.submissionsForMarking(lecturer)) {
            if (item.submissionId == extendedSubmission) {
                extensionSubmissionOnTime = (item.lateStatus == "OnTime" && item.latenessDays == 0);
            }
        }
        require(extensionSubmissionOnTime, "submission before the individual extension deadline is on time");
        // A second submission creates a new version and leaves only the newest version current.
        int resubmitted = svc.submit(student2, extendedAssignment, "resubmitted-content-ref");
        require(resubmitted == extendedSubmission, "resubmission reuses the student's assignment submission record");
        lms::data::Statement versions(
            db.handle(),
            "SELECT count(*),sum(is_current),max(version_no) FROM submission_versions WHERE submission_id=?1");
        versions.bind(1, extendedSubmission);
        require(versions.step() && versions.integer(0) == 2 && versions.integer(1) == 1 && versions.integer(2) == 2,
                "resubmission preserves history and exactly one current version");
        // Actual file upload validates a real file and stores a server-named copy, not the submitted client path.
        const std::string localFile = "lms_upload_test.txt";
        {
            std::ofstream out(localFile, std::ios::binary);
            out << "Phase 4 file upload test\n";
        }
        int fileAssignment =
            svc.createAssignment(lecturer, course, "File upload test", "Upload path", "2026-10-22T12:00:00Z", 20.0);
        const auto store = std::filesystem::path("lms_test_upload_store");
        int fileSubmission = svc.submitFile(student2, fileAssignment, localFile, store);
        lms::data::Statement stored(
            db.handle(),
            "SELECT sv.content_ref FROM submission_versions sv JOIN submissions su ON su.submission_id=sv.submission_id WHERE su.submission_id=?1");
        stored.bind(1, fileSubmission);
        require(stored.step() && std::filesystem::is_regular_file(stored.text(0)),
                "uploaded content is copied to storage and referenced by submission");
        std::filesystem::remove(localFile);
        std::filesystem::remove_all(store);
        mustFail([&] { svc.submitFile(student2, fileAssignment, "missing-file.exe", store); },
                 "missing/unsupported file is rejected");
        // Verify audit data is committed with corrected mark and includes previous final mark.
        lms::data::Statement audit(
            db.handle(),
            "SELECT actor_id,old_mark,new_mark,reason FROM mark_audit WHERE mark_id=?1 ORDER BY audit_id DESC LIMIT 1");
        audit.bind(1, corrected);
        require(audit.step(), "mark audit row exists");
        require(audit.integer(0) == lecturerId && !audit.isNull(1) && audit.real(1) == 70.0 && audit.real(2) == 72.0 &&
                    audit.text(3) == "Corrected after moderation",
                "audit fields are correct");
        // Another department's HoD cannot moderate this mark.
        lms::domain::Session forged = student;
        forged.role = lms::domain::Role::Administrator;
        mustFail([&] { svc.createDepartment(forged, "Injected"); }, "forged role session must be denied");
        int otherDep = svc.createDepartment(admin, "Arts");
        int otherHodId = svc.createUser(admin, "otherhod", "CorrectHorseBattery5!", "Other Head",
                                        lms::domain::Role::HeadOfDepartment, otherDep);
        auto otherHod = svc.login("otherhod", "CorrectHorseBattery5!");
        svc.updateUserAccount(admin, otherHodId, "Other Head Updated", lms::domain::Role::HeadOfDepartment, otherDep);
        otherHod = svc.login("otherhod", "CorrectHorseBattery5!");
        require(otherHod.fullName == "Other Head Updated", "administrator can update user account");
        mustFail([&] { svc.moderate(otherHod, mark, true, "cross-department attempt"); },
                 "cross-department moderation must be denied");
        // Regression tests: resubmission deadline rule (FR-10/BR-04) and approved-mark change guard (FR-17).
        {
            int lateAssignment = svc.createAssignment(lecturer, course, "Late rule test", "Deadline rules",
                                                      "2026-10-20T12:00:00Z", 100.0);
            int lateSub = svc.submit(student2, lateAssignment, "first-late-submission");
            lms::data::Statement lv(
                db.handle(),
                "SELECT count(*),sum(is_current),max(lateness_days) FROM submission_versions WHERE submission_id=?1");
            lv.bind(1, lateSub);
            require(lv.step() && lv.integer(0) == 1 && lv.integer(1) == 1 && lv.integer(2) == 1,
                    "first late submission within the window is still accepted");
            mustFail([&] { svc.submit(student2, lateAssignment, "late-resubmission"); },
                     "resubmission after the applicable deadline must be rejected");
            lms::data::Statement lv2(
                db.handle(),
                "SELECT count(*),sum(is_current),max(version_no) FROM submission_versions WHERE submission_id=?1");
            lv2.bind(1, lateSub);
            require(lv2.step() && lv2.integer(0) == 1 && lv2.integer(1) == 1 && lv2.integer(2) == 1,
                    "rejected resubmission leaves the version history and current version unchanged");
            // Boundary (provisional OD-02: at-deadline is on time): extended deadline for extendedAssignment is
            // 2026-10-22T12:00:00Z.
            testNow = "2026-10-22T12:00:00Z";
            svc.submit(student2, extendedAssignment, "resubmission-at-deadline");
            testNow = "2026-10-22T12:00:01Z";
            mustFail([&] { svc.submit(student2, extendedAssignment, "resubmission-one-second-late"); },
                     "resubmission one second after the deadline must be rejected");
            lms::data::Statement bv(
                db.handle(),
                "SELECT count(*),sum(is_current),max(version_no) FROM submission_versions WHERE submission_id=?1");
            bv.bind(1, extendedSubmission);
            require(bv.step() && bv.integer(0) == 3 && bv.integer(1) == 1 && bv.integer(2) == 3,
                    "only the at-deadline resubmission was added");
            testNow = "2026-10-21T12:00:00Z";
        }
        {
            int guardAssignment = svc.createAssignment(lecturer, course, "Approval guard", "Approved mark guard",
                                                       "2026-10-22T12:00:00Z", 100.0);
            int guardSub = svc.submit(student, guardAssignment, "guard-submission");
            int guardMark = svc.markSubmission(lecturer, guardSub, 90.0, "Good", "Initial mark");
            svc.moderate(hod, guardMark, true, "Approved for release");
            mustFail(
                [&] {
                    svc.markSubmission(lecturer, guardSub, 10.0, "Changed after approval",
                                       "Attempt to change an approved mark");
                },
                "an approved, unreleased mark cannot be changed without fresh moderation");
            lms::data::Statement gm(
                db.handle(),
                "SELECT final_mark,(SELECT count(*) FROM mark_audit WHERE mark_id=?1) FROM marks WHERE mark_id=?1");
            gm.bind(1, guardMark);
            require(gm.step() && gm.real(0) == 90.0 && gm.integer(1) == 1,
                    "blocked change leaves the mark and audit trail untouched");
            svc.moderate(hod, guardMark, false, "Returned for correction");
            svc.markSubmission(lecturer, guardSub, 85.0, "Revised", "Corrected after return");
            mustFail([&] { svc.release(hod, guardMark); },
                     "corrected mark cannot be released without a fresh approval");
            svc.moderate(hod, guardMark, true, "Re-approved");
            svc.release(hod, guardMark);
        }
        std::remove(path.c_str());
        std::cout
            << "PASS: authentication, role checks, setup, assignment, submission/resubmission, extension, late penalty, mark bounds, moderation gate, release, feedback visibility, audit and student/department isolation\n";
        return 0;
    } catch (const std::exception& e) {
        std::remove(path.c_str());
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
