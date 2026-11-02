#pragma once
// LmsService: Facade over the application services (Phase 3 SDD layered architecture).
//
// It is the composition root: it creates the SQLite repositories (the only objects that know about the database),
// the penalty strategy and the services, and wires them together. The console UI and the tests talk to this one
// class; every method simply delegates to the service that owns the use case.
#include "data/Database.hpp"
#include "data/SqliteRepositories.hpp"
#include "domain/Records.hpp"
#include "domain/User.hpp"
#include "services/PenaltyStrategy.hpp"
#include "services/Services.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lms::services {

class LmsService {
  public:
    explicit LmsService(data::Database& db, std::function<std::string()> clock = {});

    // Authentication (FR-01)
    bool hasAnyUser() const;
    int bootstrapAdministrator(const std::string& username, const std::string& password, const std::string& fullName);
    domain::Session login(const std::string& username, const std::string& password) const;
    // Administration (FR-03 to FR-05)
    int createDepartment(const domain::Session&, const std::string& name);
    void deactivateUser(const domain::Session&, int userId);
    void updateUserAccount(const domain::Session&, int userId, const std::string& fullName, domain::Role role,
                           int departmentId);
    void updateDepartment(const domain::Session&, int departmentId, const std::string& name);
    void updateCourse(const domain::Session&, int courseId, const std::string& code, const std::string& title,
                      int departmentId);
    int createUser(const domain::Session&, const std::string& username, const std::string& password,
                   const std::string& fullName, domain::Role role, int departmentId = 0);
    int createCourse(const domain::Session&, int departmentId, const std::string& code, const std::string& title);
    void assignLecturer(const domain::Session&, int courseId, int lecturerId);
    void enrolStudent(const domain::Session&, int courseId, int studentId);
    std::vector<std::string> listDepartments() const;
    // Assignments and extensions (FR-06 to FR-08, FR-21)
    int createAssignment(const domain::Session&, int courseId, const std::string& title, const std::string& description,
                         const std::string& deadline, double maxMark);
    void grantIndividualExtension(const domain::Session&, int assignmentId, int studentId,
                                  const std::string& revisedDeadline, const std::string& reason);
    std::vector<AssignmentView> listAssignments(const domain::Session&) const;
    // Submission (FR-09, FR-10, FR-12)
    int submit(const domain::Session&, int assignmentId, const std::string& contentRef);
    int submitFile(const domain::Session&, int assignmentId, const std::filesystem::path& sourceFile,
                   const std::filesystem::path& storageDirectory);
    std::vector<SubmissionView> submissionsForMarking(const domain::Session&) const;
    // Marking and audit (FR-11, FR-13, FR-14)
    int markSubmission(const domain::Session&, int submissionId, double rawMark, const std::string& feedback,
                       const std::string& reason);
    // Moderation, release and results (FR-15 to FR-18)
    std::vector<ModerationItemView> moderationQueue(const domain::Session&) const;
    void moderate(const domain::Session&, int markId, bool approve, const std::string& reason);
    void release(const domain::Session&, int markId);
    std::vector<ResultView> releasedResults(const domain::Session&) const;

  private:
    // Persistence (SQLite implementations of the repository interfaces). Declaration order = construction order.
    data::SqliteTransactionManager tx_;
    data::SqliteUserRepository users_;
    data::SqliteCourseRepository courses_;
    data::SqliteSubmissionRepository submissions_;
    data::SqliteMarkRepository marks_;
    data::SqliteModerationRepository moderation_;
    // Policy
    ProvisionalLinearPenaltyStrategy penalty_;
    // Application services
    AuthorizationService authz_;
    AuthenticationService authn_;
    UserAdministrationService userAdmin_;
    CourseService courseSvc_;
    AssignmentService assignmentSvc_;
    ExtensionService extensionSvc_;
    SubmissionService submissionSvc_;
    AuditService audit_;
    MarkingService marking_;
    ModerationService moderationSvc_;
    ResultQueryService results_;
};

} // namespace lms::services
