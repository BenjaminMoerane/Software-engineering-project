#include "data/SqliteRepositories.hpp"

#include <stdexcept>

namespace lms::data {
namespace {
int lastId(const Database& db) {
    return static_cast<int>(sqlite3_last_insert_rowid(db.handle()));
}
bool changedOneRow(const Database& db) {
    return sqlite3_changes(db.handle()) == 1;
}
void bindOptionalDepartment(Statement& q, int index, int departmentId) {
    if (departmentId > 0)
        q.bind(index, departmentId);
    else
        q.bindNull(index);
}
// Latest moderation decision for a mark, used as a scalar sub-query in several statements.
constexpr const char* kLatestDecision = "(SELECT md.decision FROM moderation_decisions md WHERE md.mark_id=m.mark_id "
                                        "ORDER BY md.decided_at DESC,md.moderation_id DESC LIMIT 1)";

class SqliteTransaction final : public repo::ITransaction {
  public:
    explicit SqliteTransaction(sqlite3* db) : tx_(db) {}
    void commit() override { tx_.commit(); }

  private:
    Transaction tx_;
};
} // namespace

std::unique_ptr<repo::ITransaction> SqliteTransactionManager::begin() {
    return std::make_unique<SqliteTransaction>(db_.handle());
}

// ---------------------------------------------------------------- users
int SqliteUserRepository::countUsers() const {
    Statement q(db_.handle(), "SELECT count(*) FROM users");
    q.step();
    return q.integer(0);
}
std::optional<repo::UserCredentials> SqliteUserRepository::findCredentials(const std::string& username) const {
    Statement q(db_.handle(), "SELECT user_id,username,full_name,role,COALESCE(department_id,0),password_hash,"
                              "password_salt,active FROM users WHERE username=?1");
    q.bind(1, username);
    if (!q.step())
        return std::nullopt;
    return repo::UserCredentials{q.integer(0), q.text(1), q.text(2), q.text(3),
                                 q.integer(4), q.text(5), q.text(6), q.integer(7) == 1};
}
std::optional<repo::AccountState> SqliteUserRepository::findAccountState(int userId) const {
    Statement q(db_.handle(), "SELECT active,role,COALESCE(department_id,0) FROM users WHERE user_id=?1");
    q.bind(1, userId);
    if (!q.step())
        return std::nullopt;
    return repo::AccountState{q.integer(0) == 1, q.text(1), q.integer(2)};
}
int SqliteUserRepository::insert(const repo::NewUser& u) {
    Statement q(db_.handle(), "INSERT INTO users(username,password_hash,password_salt,role,full_name,department_id) "
                              "VALUES(?1,?2,?3,?4,?5,?6)");
    q.bind(1, u.username);
    q.bind(2, u.passwordHash);
    q.bind(3, u.passwordSalt);
    q.bind(4, u.role);
    q.bind(5, u.fullName);
    bindOptionalDepartment(q, 6, u.departmentId);
    q.execute();
    return lastId(db_);
}
bool SqliteUserRepository::deactivate(int userId) {
    Statement q(db_.handle(), "UPDATE users SET active=0 WHERE user_id=?1 AND active=1");
    q.bind(1, userId);
    q.execute();
    return changedOneRow(db_);
}
bool SqliteUserRepository::update(int userId, const std::string& fullName, const std::string& role, int departmentId) {
    Statement q(db_.handle(), "UPDATE users SET full_name=?1,role=?2,department_id=?3 WHERE user_id=?4");
    q.bind(1, fullName);
    q.bind(2, role);
    bindOptionalDepartment(q, 3, departmentId);
    q.bind(4, userId);
    q.execute();
    return changedOneRow(db_);
}

// ---------------------------------------------------------------- courses, assignments, extensions
int SqliteCourseRepository::insertDepartment(const std::string& name) {
    Statement q(db_.handle(), "INSERT INTO departments(name) VALUES(?1)");
    q.bind(1, name);
    q.execute();
    return lastId(db_);
}
bool SqliteCourseRepository::updateDepartment(int departmentId, const std::string& name) {
    Statement q(db_.handle(), "UPDATE departments SET name=?1 WHERE department_id=?2");
    q.bind(1, name);
    q.bind(2, departmentId);
    q.execute();
    return changedOneRow(db_);
}
std::vector<std::pair<int, std::string>> SqliteCourseRepository::listDepartments() const {
    Statement q(db_.handle(), "SELECT department_id,name FROM departments ORDER BY name");
    std::vector<std::pair<int, std::string>> out;
    while (q.step())
        out.emplace_back(q.integer(0), q.text(1));
    return out;
}
int SqliteCourseRepository::insertCourse(int departmentId, const std::string& code, const std::string& title) {
    Statement q(db_.handle(), "INSERT INTO courses(department_id,course_code,title) VALUES(?1,?2,?3)");
    q.bind(1, departmentId);
    q.bind(2, code);
    q.bind(3, title);
    q.execute();
    return lastId(db_);
}
bool SqliteCourseRepository::updateCourse(int courseId, const std::string& code, const std::string& title,
                                          int departmentId) {
    Statement q(db_.handle(), "UPDATE courses SET course_code=?1,title=?2,department_id=?3 WHERE course_id=?4");
    q.bind(1, code);
    q.bind(2, title);
    q.bind(3, departmentId);
    q.bind(4, courseId);
    q.execute();
    return changedOneRow(db_);
}
std::optional<int> SqliteCourseRepository::departmentOfCourse(int courseId) const {
    Statement q(db_.handle(), "SELECT department_id FROM courses WHERE course_id=?1");
    q.bind(1, courseId);
    if (!q.step())
        return std::nullopt;
    return q.integer(0);
}
void SqliteCourseRepository::assignLecturer(int courseId, int lecturerId) {
    Statement q(db_.handle(), "INSERT INTO course_lecturers(course_id,lecturer_id) VALUES(?1,?2)");
    q.bind(1, courseId);
    q.bind(2, lecturerId);
    q.execute();
}
void SqliteCourseRepository::enrolStudent(int courseId, int studentId) {
    Statement q(db_.handle(), "INSERT INTO enrolments(course_id,student_id) VALUES(?1,?2)");
    q.bind(1, courseId);
    q.bind(2, studentId);
    q.execute();
}
bool SqliteCourseRepository::isLecturerAssigned(int lecturerId, int courseId) const {
    Statement q(db_.handle(), "SELECT 1 FROM course_lecturers WHERE lecturer_id=?1 AND course_id=?2");
    q.bind(1, lecturerId);
    q.bind(2, courseId);
    return q.step();
}
bool SqliteCourseRepository::isStudentEnrolled(int studentId, int courseId) const {
    Statement q(db_.handle(), "SELECT 1 FROM enrolments WHERE student_id=?1 AND course_id=?2");
    q.bind(1, studentId);
    q.bind(2, courseId);
    return q.step();
}
int SqliteCourseRepository::insertAssignment(const repo::NewAssignment& a) {
    Statement q(db_.handle(), "INSERT INTO assignments(course_id,created_by,title,description,deadline,max_mark) "
                              "VALUES(?1,?2,?3,?4,?5,?6)");
    q.bind(1, a.courseId);
    q.bind(2, a.createdBy);
    q.bind(3, a.title);
    q.bind(4, a.description);
    q.bind(5, a.deadline);
    q.bind(6, a.maxMark);
    q.execute();
    return lastId(db_);
}
std::optional<repo::AssignmentInfo> SqliteCourseRepository::findActiveAssignment(int assignmentId) const {
    Statement q(db_.handle(),
                "SELECT course_id,deadline,max_mark FROM assignments WHERE assignment_id=?1 AND active=1");
    q.bind(1, assignmentId);
    if (!q.step())
        return std::nullopt;
    return repo::AssignmentInfo{q.integer(0), q.text(1), q.real(2)};
}
std::vector<repo::AssignmentView> SqliteCourseRepository::listAssignmentsVisibleTo(int userId, const std::string& role,
                                                                                   int departmentId) const {
    Statement q(db_.handle(),
                "SELECT a.assignment_id,c.course_code,a.title,a.deadline,a.max_mark,COALESCE(e.revised_deadline,'') "
                "FROM assignments a JOIN courses c ON c.course_id=a.course_id "
                "LEFT JOIN extensions e ON e.assignment_id=a.assignment_id AND e.student_id=?1 "
                "WHERE a.active=1 "
                "AND (?2<>'Student' OR EXISTS(SELECT 1 FROM enrolments en WHERE en.course_id=a.course_id "
                "AND en.student_id=?1)) "
                "AND (?2<>'Lecturer' OR EXISTS(SELECT 1 FROM course_lecturers cl WHERE cl.course_id=a.course_id "
                "AND cl.lecturer_id=?1)) "
                "AND (?2<>'HeadOfDepartment' OR c.department_id=?3) "
                "ORDER BY a.deadline,a.assignment_id");
    q.bind(1, userId);
    q.bind(2, role);
    q.bind(3, departmentId);
    std::vector<repo::AssignmentView> out;
    while (q.step())
        out.push_back({q.integer(0), q.text(1), q.text(2), q.text(3), q.real(4), q.text(5)});
    return out;
}
void SqliteCourseRepository::upsertExtension(int assignmentId, int studentId, int authorisedBy,
                                             const std::string& revisedDeadline, const std::string& reason) {
    Statement q(db_.handle(), "INSERT INTO extensions(assignment_id,student_id,authorised_by,revised_deadline,reason) "
                              "VALUES(?1,?2,?3,?4,?5) ON CONFLICT(assignment_id,student_id) DO UPDATE SET "
                              "authorised_by=excluded.authorised_by,revised_deadline=excluded.revised_deadline,"
                              "reason=excluded.reason,created_at=strftime('%Y-%m-%dT%H:%M:%fZ','now')");
    q.bind(1, assignmentId);
    q.bind(2, studentId);
    q.bind(3, authorisedBy);
    q.bind(4, revisedDeadline);
    q.bind(5, reason);
    q.execute();
}
std::optional<std::string> SqliteCourseRepository::findExtensionDeadline(int assignmentId, int studentId) const {
    Statement q(db_.handle(), "SELECT revised_deadline FROM extensions WHERE assignment_id=?1 AND student_id=?2");
    q.bind(1, assignmentId);
    q.bind(2, studentId);
    if (!q.step())
        return std::nullopt;
    return q.text(0);
}

// ---------------------------------------------------------------- submissions
std::optional<repo::ExistingSubmission> SqliteSubmissionRepository::find(int assignmentId, int studentId) const {
    Statement q(db_.handle(),
                "SELECT submission_id,current_version_no FROM submissions WHERE assignment_id=?1 AND student_id=?2");
    q.bind(1, assignmentId);
    q.bind(2, studentId);
    if (!q.step())
        return std::nullopt;
    return repo::ExistingSubmission{q.integer(0), q.integer(1)};
}
int SqliteSubmissionRepository::saveVersion(const repo::NewSubmissionVersion& v) {
    int submissionId = v.submissionId;
    if (submissionId == 0) {
        Statement ins(db_.handle(),
                      "INSERT INTO submissions(assignment_id,student_id,current_version_no) VALUES(?1,?2,0)");
        ins.bind(1, v.assignmentId);
        ins.bind(2, v.studentId);
        ins.execute();
        submissionId = lastId(db_);
    }
    Statement retire(db_.handle(),
                     "UPDATE submission_versions SET is_current=0 WHERE submission_id=?1 AND is_current=1");
    retire.bind(1, submissionId);
    retire.execute();
    Statement ins(db_.handle(), "INSERT INTO submission_versions(submission_id,version_no,submitted_at,content_ref,"
                                "deadline_used,lateness_days,late_status,is_current) VALUES(?1,?2,?3,?4,?5,?6,?7,1)");
    ins.bind(1, submissionId);
    ins.bind(2, v.versionNo);
    ins.bind(3, v.submittedAt);
    ins.bind(4, v.contentRef);
    ins.bind(5, v.deadlineUsed);
    ins.bind(6, v.latenessDays);
    ins.bind(7, v.lateStatus);
    ins.execute();
    Statement pointer(db_.handle(), "UPDATE submissions SET current_version_no=?1 WHERE submission_id=?2");
    pointer.bind(1, v.versionNo);
    pointer.bind(2, submissionId);
    pointer.execute();
    return submissionId;
}
std::optional<repo::MarkingContext> SqliteSubmissionRepository::findMarkingContext(int submissionId) const {
    Statement q(db_.handle(),
                "SELECT a.course_id,a.max_mark,sv.version_id,sv.lateness_days,sv.late_status FROM submissions su "
                "JOIN assignments a ON a.assignment_id=su.assignment_id "
                "JOIN submission_versions sv ON sv.submission_id=su.submission_id AND sv.is_current=1 "
                "WHERE su.submission_id=?1");
    q.bind(1, submissionId);
    if (!q.step())
        return std::nullopt;
    return repo::MarkingContext{q.integer(0), q.real(1), q.integer(2), q.integer(3), q.text(4)};
}
std::vector<repo::SubmissionView> SqliteSubmissionRepository::markingQueueFor(int lecturerId) const {
    Statement q(db_.handle(),
                std::string("SELECT su.submission_id,sv.version_id,su.student_id,a.assignment_id,sv.version_no,"
                            "u.full_name,a.title,sv.submitted_at,sv.late_status,sv.lateness_days,sv.content_ref,"
                            "a.max_mark FROM submissions su "
                            "JOIN submission_versions sv ON sv.submission_id=su.submission_id AND sv.is_current=1 "
                            "JOIN assignments a ON a.assignment_id=su.assignment_id "
                            "JOIN users u ON u.user_id=su.student_id "
                            "JOIN course_lecturers cl ON cl.course_id=a.course_id AND cl.lecturer_id=?1 "
                            "LEFT JOIN marks m ON m.submission_id=su.submission_id AND m.current=1 "
                            "WHERE m.mark_id IS NULL OR ") +
                    kLatestDecision + "='ReturnedForCorrection' ORDER BY sv.submitted_at");
    q.bind(1, lecturerId);
    std::vector<repo::SubmissionView> out;
    while (q.step())
        out.push_back({q.integer(0), q.integer(1), q.integer(2), q.integer(3), q.integer(4), q.integer(9), q.text(5),
                       q.text(6), q.text(7), q.text(8), q.text(10), q.real(11)});
    return out;
}

// ---------------------------------------------------------------- marks, feedback and audit
std::optional<repo::CurrentMark> SqliteMarkRepository::findCurrent(int submissionId) const {
    Statement q(db_.handle(), "SELECT mark_id,final_mark FROM marks WHERE submission_id=?1 AND current=1");
    q.bind(1, submissionId);
    if (!q.step())
        return std::nullopt;
    return repo::CurrentMark{q.integer(0), q.real(1)};
}
void SqliteMarkRepository::updateCurrent(int markId, const repo::MarkWrite& m) {
    Statement update(db_.handle(),
                     "UPDATE marks SET version_id=?1,raw_mark=?2,penalty_amount=?3,final_mark=?4,entered_by=?5,"
                     "marked_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE mark_id=?6");
    update.bind(1, m.versionId);
    update.bind(2, m.rawMark);
    update.bind(3, m.penaltyAmount);
    update.bind(4, m.finalMark);
    update.bind(5, m.enteredBy);
    update.bind(6, markId);
    update.execute();
    Statement fb(db_.handle(), "UPDATE feedback SET body=?1,author_id=?2,"
                               "created_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE mark_id=?3");
    fb.bind(1, m.feedback);
    fb.bind(2, m.enteredBy);
    fb.bind(3, markId);
    fb.execute();
}
int SqliteMarkRepository::insertCurrent(const repo::MarkWrite& m) {
    Statement ins(db_.handle(), "INSERT INTO marks(submission_id,version_id,raw_mark,penalty_amount,final_mark,"
                                "entered_by,current) VALUES(?1,?2,?3,?4,?5,?6,1)");
    ins.bind(1, m.submissionId);
    ins.bind(2, m.versionId);
    ins.bind(3, m.rawMark);
    ins.bind(4, m.penaltyAmount);
    ins.bind(5, m.finalMark);
    ins.bind(6, m.enteredBy);
    ins.execute();
    const int markId = lastId(db_);
    Statement fb(db_.handle(), "INSERT INTO feedback(mark_id,body,author_id) VALUES(?1,?2,?3)");
    fb.bind(1, markId);
    fb.bind(2, m.feedback);
    fb.bind(3, m.enteredBy);
    fb.execute();
    return markId;
}
void SqliteMarkRepository::appendAudit(const repo::MarkAuditEntry& e) {
    Statement q(db_.handle(),
                "INSERT INTO mark_audit(mark_id,actor_id,old_mark,new_mark,reason) VALUES(?1,?2,?3,?4,?5)");
    q.bind(1, e.markId);
    q.bind(2, e.actorId);
    if (e.oldMark)
        q.bind(3, *e.oldMark);
    else
        q.bindNull(3);
    q.bind(4, e.newMark);
    q.bind(5, e.reason);
    q.execute();
}

// ---------------------------------------------------------------- moderation and release
std::optional<int> SqliteModerationRepository::departmentOfCurrentMark(int markId) const {
    Statement q(db_.handle(), "SELECT c.department_id FROM marks m "
                              "JOIN submissions su ON su.submission_id=m.submission_id "
                              "JOIN assignments a ON a.assignment_id=su.assignment_id "
                              "JOIN courses c ON c.course_id=a.course_id WHERE m.mark_id=?1 AND m.current=1");
    q.bind(1, markId);
    if (!q.step())
        return std::nullopt;
    return q.integer(0);
}
std::optional<std::string> SqliteModerationRepository::latestDecision(int markId) const {
    Statement q(db_.handle(), "SELECT decision FROM moderation_decisions WHERE mark_id=?1 "
                              "ORDER BY decided_at DESC,moderation_id DESC LIMIT 1");
    q.bind(1, markId);
    if (!q.step())
        return std::nullopt;
    return q.text(0);
}
bool SqliteModerationRepository::isReleased(int markId) const {
    Statement q(db_.handle(), "SELECT 1 FROM release_records WHERE mark_id=?1");
    q.bind(1, markId);
    return q.step();
}
void SqliteModerationRepository::insertDecision(int markId, int hodId, const std::string& decision,
                                                const std::string& reason) {
    Statement q(db_.handle(), "INSERT INTO moderation_decisions(mark_id,hod_id,decision,reason) VALUES(?1,?2,?3,?4)");
    q.bind(1, markId);
    q.bind(2, hodId);
    q.bind(3, decision);
    q.bind(4, reason);
    q.execute();
}
void SqliteModerationRepository::insertRelease(int markId, int releasedBy) {
    Statement q(db_.handle(), "INSERT INTO release_records(mark_id,released_by) VALUES(?1,?2)");
    q.bind(1, markId);
    q.bind(2, releasedBy);
    q.execute();
}
std::vector<repo::ModerationQueueRow> SqliteModerationRepository::unreleasedCurrentMarksFor(int departmentId) const {
    Statement q(db_.handle(),
                std::string("SELECT m.mark_id,su.submission_id,u.full_name,c.course_code,a.title,sv.late_status,"
                            "COALESCE(f.body,''),m.raw_mark,m.penalty_amount,m.final_mark,a.max_mark,") +
                    kLatestDecision +
                    ",m.marked_at>(SELECT md.decided_at FROM moderation_decisions md WHERE md.mark_id=m.mark_id "
                    "ORDER BY md.decided_at DESC,md.moderation_id DESC LIMIT 1) "
                    "FROM marks m JOIN submissions su ON su.submission_id=m.submission_id "
                    "JOIN submission_versions sv ON sv.version_id=m.version_id "
                    "JOIN assignments a ON a.assignment_id=su.assignment_id "
                    "JOIN courses c ON c.course_id=a.course_id JOIN users u ON u.user_id=su.student_id "
                    "LEFT JOIN feedback f ON f.mark_id=m.mark_id "
                    "WHERE m.current=1 AND c.department_id=?1 "
                    "AND NOT EXISTS (SELECT 1 FROM release_records rr WHERE rr.mark_id=m.mark_id) "
                    "ORDER BY m.marked_at,m.mark_id");
    q.bind(1, departmentId);
    std::vector<repo::ModerationQueueRow> out;
    while (q.step()) {
        repo::ModerationQueueRow row;
        row.item = {q.integer(0), q.integer(1), q.text(2), q.text(3), q.text(4), q.text(5),
                    q.text(6),    "",           q.real(7), q.real(8), q.real(9), q.real(10)};
        row.latestDecision = q.isNull(11) ? "" : q.text(11);
        row.correctedSinceLatestDecision = !q.isNull(12) && q.integer(12) == 1;
        out.push_back(std::move(row));
    }
    return out;
}
std::vector<repo::ResultView> SqliteModerationRepository::releasedResultsFor(int studentId) const {
    Statement q(db_.handle(), "SELECT a.assignment_id,a.title,c.course_code,m.final_mark,a.max_mark,f.body "
                              "FROM release_records rr JOIN marks m ON m.mark_id=rr.mark_id AND m.current=1 "
                              "JOIN submissions su ON su.submission_id=m.submission_id "
                              "JOIN assignments a ON a.assignment_id=su.assignment_id "
                              "JOIN courses c ON c.course_id=a.course_id "
                              "LEFT JOIN feedback f ON f.mark_id=m.mark_id WHERE su.student_id=?1 ORDER BY a.title");
    q.bind(1, studentId);
    std::vector<repo::ResultView> out;
    while (q.step())
        out.push_back({q.integer(0), q.text(1), q.text(2), q.text(5), q.real(3), q.real(4)});
    return out;
}

} // namespace lms::data
