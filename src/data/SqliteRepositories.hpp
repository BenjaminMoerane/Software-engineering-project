#pragma once
// SQLite implementations of the repository interfaces. This directory (src/data/) is the only place in the
// application that contains SQL text or calls the SQLite C API; every statement is parameterised.
#include "data/Database.hpp"
#include "repositories/Repositories.hpp"

namespace lms::data {
namespace repo = lms::repositories;

class SqliteTransactionManager final : public repo::ITransactionManager {
  public:
    explicit SqliteTransactionManager(Database& db) : db_(db) {}
    std::unique_ptr<repo::ITransaction> begin() override;

  private:
    Database& db_;
};

class SqliteUserRepository final : public repo::IUserRepository {
  public:
    explicit SqliteUserRepository(Database& db) : db_(db) {}
    int countUsers() const override;
    std::optional<repo::UserCredentials> findCredentials(const std::string& username) const override;
    std::optional<repo::AccountState> findAccountState(int userId) const override;
    int insert(const repo::NewUser& user) override;
    bool deactivate(int userId) override;
    bool update(int userId, const std::string& fullName, const std::string& role, int departmentId) override;

  private:
    Database& db_;
};

class SqliteCourseRepository final : public repo::ICourseRepository {
  public:
    explicit SqliteCourseRepository(Database& db) : db_(db) {}
    int insertDepartment(const std::string& name) override;
    bool updateDepartment(int departmentId, const std::string& name) override;
    std::vector<std::pair<int, std::string>> listDepartments() const override;
    int insertCourse(int departmentId, const std::string& code, const std::string& title) override;
    bool updateCourse(int courseId, const std::string& code, const std::string& title, int departmentId) override;
    std::optional<int> departmentOfCourse(int courseId) const override;
    void assignLecturer(int courseId, int lecturerId) override;
    void enrolStudent(int courseId, int studentId) override;
    bool isLecturerAssigned(int lecturerId, int courseId) const override;
    bool isStudentEnrolled(int studentId, int courseId) const override;
    int insertAssignment(const repo::NewAssignment& assignment) override;
    std::optional<repo::AssignmentInfo> findActiveAssignment(int assignmentId) const override;
    std::vector<repo::AssignmentView> listAssignmentsVisibleTo(int userId, const std::string& role,
                                                               int departmentId) const override;
    void upsertExtension(int assignmentId, int studentId, int authorisedBy, const std::string& revisedDeadline,
                         const std::string& reason) override;
    std::optional<std::string> findExtensionDeadline(int assignmentId, int studentId) const override;

  private:
    Database& db_;
};

class SqliteSubmissionRepository final : public repo::ISubmissionRepository {
  public:
    explicit SqliteSubmissionRepository(Database& db) : db_(db) {}
    std::optional<repo::ExistingSubmission> find(int assignmentId, int studentId) const override;
    int saveVersion(const repo::NewSubmissionVersion& version) override;
    std::optional<repo::MarkingContext> findMarkingContext(int submissionId) const override;
    std::vector<repo::SubmissionView> markingQueueFor(int lecturerId) const override;

  private:
    Database& db_;
};

class SqliteMarkRepository final : public repo::IMarkRepository {
  public:
    explicit SqliteMarkRepository(Database& db) : db_(db) {}
    std::optional<repo::CurrentMark> findCurrent(int submissionId) const override;
    void updateCurrent(int markId, const repo::MarkWrite& mark) override;
    int insertCurrent(const repo::MarkWrite& mark) override;
    void appendAudit(const repo::MarkAuditEntry& entry) override;

  private:
    Database& db_;
};

class SqliteModerationRepository final : public repo::IModerationRepository {
  public:
    explicit SqliteModerationRepository(Database& db) : db_(db) {}
    std::optional<int> departmentOfCurrentMark(int markId) const override;
    std::optional<std::string> latestDecision(int markId) const override;
    bool isReleased(int markId) const override;
    void insertDecision(int markId, int hodId, const std::string& decision, const std::string& reason) override;
    void insertRelease(int markId, int releasedBy) override;
    std::vector<repo::ModerationQueueRow> unreleasedCurrentMarksFor(int departmentId) const override;
    std::vector<repo::ResultView> releasedResultsFor(int studentId) const override;

  private:
    Database& db_;
};

} // namespace lms::data
