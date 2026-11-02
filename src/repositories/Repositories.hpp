#pragma once
// Repository interfaces (Phase 3 SDD: IUserRepository, ICourseRepository, ISubmissionRepository, IMarkRepository,
// IModerationRepository) plus ITransactionManager, the unit-of-work boundary that lets a service make several
// repository calls atomic (e.g. mark update + audit event, FR-14/NFR-06) without seeing SQLite.
//
// Rule: application services depend only on these interfaces. All SQL lives in src/data/ (enforced by
// tests/architecture/check_sql_isolation.py, run by CTest and CI).
#include "domain/Records.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lms::repositories {
using namespace lms::domain;

class ITransaction {
  public:
    ITransaction() = default;
    virtual ~ITransaction() = default;
    ITransaction(const ITransaction&) = delete; // used by reference only: no slicing
    ITransaction& operator=(const ITransaction&) = delete;
    ITransaction(ITransaction&&) = delete;
    ITransaction& operator=(ITransaction&&) = delete;
    virtual void commit() = 0; // destroying an uncommitted transaction rolls it back
};
class ITransactionManager {
  public:
    ITransactionManager() = default;
    virtual ~ITransactionManager() = default;
    ITransactionManager(const ITransactionManager&) = delete; // used by reference only: no slicing
    ITransactionManager& operator=(const ITransactionManager&) = delete;
    ITransactionManager(ITransactionManager&&) = delete;
    ITransactionManager& operator=(ITransactionManager&&) = delete;
    virtual std::unique_ptr<ITransaction> begin() = 0;
};

class IUserRepository {
  public:
    IUserRepository() = default;
    virtual ~IUserRepository() = default;
    IUserRepository(const IUserRepository&) = delete; // used by reference only: no slicing
    IUserRepository& operator=(const IUserRepository&) = delete;
    IUserRepository(IUserRepository&&) = delete;
    IUserRepository& operator=(IUserRepository&&) = delete;
    virtual int countUsers() const = 0;
    virtual std::optional<UserCredentials> findCredentials(const std::string& username) const = 0;
    virtual std::optional<AccountState> findAccountState(int userId) const = 0;
    virtual int insert(const NewUser& user) = 0;
    virtual bool deactivate(int userId) = 0; // false when no active account was changed
    virtual bool update(int userId, const std::string& fullName, const std::string& role, int departmentId) = 0;
};

// Courses own departments, teaching assignments, enrolments, assignments and individual extensions.
class ICourseRepository {
  public:
    ICourseRepository() = default;
    virtual ~ICourseRepository() = default;
    ICourseRepository(const ICourseRepository&) = delete; // used by reference only: no slicing
    ICourseRepository& operator=(const ICourseRepository&) = delete;
    ICourseRepository(ICourseRepository&&) = delete;
    ICourseRepository& operator=(ICourseRepository&&) = delete;
    virtual int insertDepartment(const std::string& name) = 0;
    virtual bool updateDepartment(int departmentId, const std::string& name) = 0;
    virtual std::vector<std::pair<int, std::string>> listDepartments() const = 0;
    virtual int insertCourse(int departmentId, const std::string& code, const std::string& title) = 0;
    virtual bool updateCourse(int courseId, const std::string& code, const std::string& title,
                              int departmentId) = 0;
    virtual std::optional<int> departmentOfCourse(int courseId) const = 0;
    virtual void assignLecturer(int courseId, int lecturerId) = 0;
    virtual void enrolStudent(int courseId, int studentId) = 0;
    virtual bool isLecturerAssigned(int lecturerId, int courseId) const = 0;
    virtual bool isStudentEnrolled(int studentId, int courseId) const = 0;
    virtual int insertAssignment(const NewAssignment& assignment) = 0;
    virtual std::optional<AssignmentInfo> findActiveAssignment(int assignmentId) const = 0;
    virtual std::vector<AssignmentView> listAssignmentsVisibleTo(int userId, const std::string& role,
                                                                 int departmentId) const = 0;
    virtual void upsertExtension(int assignmentId, int studentId, int authorisedBy, const std::string& revisedDeadline,
                                 const std::string& reason) = 0;
    virtual std::optional<std::string> findExtensionDeadline(int assignmentId, int studentId) const = 0;
};

class ISubmissionRepository {
  public:
    ISubmissionRepository() = default;
    virtual ~ISubmissionRepository() = default;
    ISubmissionRepository(const ISubmissionRepository&) = delete; // used by reference only: no slicing
    ISubmissionRepository& operator=(const ISubmissionRepository&) = delete;
    ISubmissionRepository(ISubmissionRepository&&) = delete;
    ISubmissionRepository& operator=(ISubmissionRepository&&) = delete;
    virtual std::optional<ExistingSubmission> find(int assignmentId, int studentId) const = 0;
    // Creates the logical submission if needed, retires the previous current version, inserts the new immutable
    // version and moves the current-version pointer. Must be called inside a transaction. Returns submission id.
    virtual int saveVersion(const NewSubmissionVersion& version) = 0;
    virtual std::optional<MarkingContext> findMarkingContext(int submissionId) const = 0;
    virtual std::vector<SubmissionView> markingQueueFor(int lecturerId) const = 0;
};

class IMarkRepository {
  public:
    IMarkRepository() = default;
    virtual ~IMarkRepository() = default;
    IMarkRepository(const IMarkRepository&) = delete; // used by reference only: no slicing
    IMarkRepository& operator=(const IMarkRepository&) = delete;
    IMarkRepository(IMarkRepository&&) = delete;
    IMarkRepository& operator=(IMarkRepository&&) = delete;
    virtual std::optional<CurrentMark> findCurrent(int submissionId) const = 0;
    virtual void updateCurrent(int markId, const MarkWrite& mark) = 0; // mark + feedback rows
    virtual int insertCurrent(const MarkWrite& mark) = 0;              // mark + feedback rows
    virtual void appendAudit(const MarkAuditEntry& entry) = 0;
};

class IModerationRepository {
  public:
    IModerationRepository() = default;
    virtual ~IModerationRepository() = default;
    IModerationRepository(const IModerationRepository&) = delete; // used by reference only: no slicing
    IModerationRepository& operator=(const IModerationRepository&) = delete;
    IModerationRepository(IModerationRepository&&) = delete;
    IModerationRepository& operator=(IModerationRepository&&) = delete;
    virtual std::optional<int> departmentOfCurrentMark(int markId) const = 0;
    virtual std::optional<std::string> latestDecision(int markId) const = 0;
    virtual bool isReleased(int markId) const = 0;
    virtual void insertDecision(int markId, int hodId, const std::string& decision, const std::string& reason) = 0;
    virtual void insertRelease(int markId, int releasedBy) = 0;
    virtual std::vector<ModerationQueueRow> unreleasedCurrentMarksFor(int departmentId) const = 0;
    virtual std::vector<ResultView> releasedResultsFor(int studentId) const = 0;
};

} // namespace lms::repositories
