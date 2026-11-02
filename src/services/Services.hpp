#pragma once
// Application-services layer (Phase 3 SDD section 2 and 3). Each service coordinates one area of use cases,
// enforces role/scope/state preconditions, and depends only on repository *interfaces* (Dependency Inversion).
// LmsService (LmsService.hpp) composes these services and is the single entry point used by the console UI.
#include "domain/User.hpp"
#include "repositories/Repositories.hpp"
#include "services/PenaltyStrategy.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lms::services {
using domain::AssignmentView;
using domain::ModerationItemView;
using domain::ResultView;
using domain::Role;
using domain::Session;
using domain::SubmissionView;
namespace repo = lms::repositories;

// FR-02, NFR-05: role, account-state and course-scope checks, re-validated against storage on every call.
class AuthorizationService {
  public:
    AuthorizationService(const repo::IUserRepository& users, const repo::ICourseRepository& courses)
        : users_(users), courses_(courses) {}
    void requireRole(const Session& session, Role role) const;
    void requireValidSession(const Session& session) const; // any role, used for read-only listings
    bool isLecturerAssigned(int lecturerId, int courseId) const;
    bool isStudentEnrolled(int studentId, int courseId) const;
    void requireLecturerAssigned(int lecturerId, int courseId) const;

  private:
    const repo::IUserRepository& users_;
    const repo::ICourseRepository& courses_;
};

// FR-01, FR-20, NFR-04: first-run bootstrap and login with salted scrypt hashes.
class AuthenticationService {
  public:
    explicit AuthenticationService(repo::IUserRepository& users) : users_(users) {}
    bool hasAnyUser() const;
    int bootstrapAdministrator(const std::string& username, const std::string& password, const std::string& fullName);
    Session login(const std::string& username, const std::string& password) const;

  private:
    repo::IUserRepository& users_;
};

// FR-03: account life cycle.
class UserAdministrationService {
  public:
    UserAdministrationService(repo::IUserRepository& users, const AuthorizationService& authz)
        : users_(users), authz_(authz) {}
    int createUser(const Session&, const std::string& username, const std::string& password,
                   const std::string& fullName, Role role, int departmentId);
    void updateUserAccount(const Session&, int userId, const std::string& fullName, Role role, int departmentId);
    void deactivateUser(const Session&, int userId);

  private:
    repo::IUserRepository& users_;
    const AuthorizationService& authz_;
};

// FR-04, FR-05: departments, courses, teaching assignments and enrolments.
class CourseService {
  public:
    CourseService(repo::ICourseRepository& courses, const repo::IUserRepository& users,
                  const AuthorizationService& authz)
        : courses_(courses), users_(users), authz_(authz) {}
    int createDepartment(const Session&, const std::string& name);
    void updateDepartment(const Session&, int departmentId, const std::string& name);
    std::vector<std::string> listDepartments() const;
    int createCourse(const Session&, int departmentId, const std::string& code, const std::string& title);
    void updateCourse(const Session&, int courseId, const std::string& code, const std::string& title,
                      int departmentId);
    void assignLecturer(const Session&, int courseId, int lecturerId);
    void enrolStudent(const Session&, int courseId, int studentId);

  private:
    void requireActiveTarget(int userId, const char* role, const char* message) const;
    repo::ICourseRepository& courses_;
    const repo::IUserRepository& users_;
    const AuthorizationService& authz_;
};

// FR-06, FR-08, FR-21: assignment set-up and role-scoped assignment lists.
class AssignmentService {
  public:
    AssignmentService(repo::ICourseRepository& courses, const AuthorizationService& authz)
        : courses_(courses), authz_(authz) {}
    int createAssignment(const Session&, int courseId, const std::string& title, const std::string& description,
                         const std::string& deadline, double maxMark);
    std::vector<AssignmentView> listAssignments(const Session&) const;

  private:
    repo::ICourseRepository& courses_;
    const AuthorizationService& authz_;
};

// FR-07: individual extensions (class-wide extensions are CR1 / v1.1).
class ExtensionService {
  public:
    ExtensionService(repo::ICourseRepository& courses, const AuthorizationService& authz)
        : courses_(courses), authz_(authz) {}
    void grantIndividualExtension(const Session&, int assignmentId, int studentId, const std::string& revisedDeadline,
                                  const std::string& reason);
    // CR1 / FR-23: extends the deadline for every student enrolled in the assignment's course. Returns the record id.
    int grantClassExtension(const Session&, int assignmentId, const std::string& revisedDeadline,
                            const std::string& reason);

  private:
    repo::ICourseRepository& courses_;
    const AuthorizationService& authz_;
};

// FR-09, FR-10, FR-12: submission, resubmission, file intake and the lecturer marking queue.
class SubmissionService {
  public:
    using Clock = std::function<std::string()>;
    SubmissionService(const repo::ICourseRepository& courses, repo::ISubmissionRepository& submissions,
                      repo::ITransactionManager& tx, const AuthorizationService& authz, Clock clock)
        : courses_(courses), submissions_(submissions), tx_(tx), authz_(authz), clock_(std::move(clock)) {}
    int submit(const Session&, int assignmentId, const std::string& contentRef);
    int submitFile(const Session&, int assignmentId, const std::filesystem::path& sourceFile,
                   const std::filesystem::path& storageDirectory);
    std::vector<SubmissionView> submissionsForMarking(const Session&) const;

  private:
    const repo::ICourseRepository& courses_;
    repo::ISubmissionRepository& submissions_;
    repo::ITransactionManager& tx_;
    const AuthorizationService& authz_;
    Clock clock_;
};

// FR-14, NFR-06: builds and appends mark-audit events; always called inside the caller's transaction.
class AuditService {
  public:
    explicit AuditService(repo::IMarkRepository& marks) : marks_(marks) {}
    void recordMarkChange(int markId, int actorId, std::optional<double> oldMark, double newMark,
                          const std::string& reason);

  private:
    repo::IMarkRepository& marks_;
};

// FR-11, FR-13, FR-14: marking, feedback, penalty application and audit in one transaction.
class MarkingService {
  public:
    MarkingService(const repo::ISubmissionRepository& submissions, repo::IMarkRepository& marks,
                   const repo::IModerationRepository& moderation, repo::ITransactionManager& tx,
                   const AuthorizationService& authz, AuditService& audit, const IPenaltyStrategy& penalty)
        : submissions_(submissions), marks_(marks), moderation_(moderation), tx_(tx), authz_(authz), audit_(audit),
          penalty_(penalty) {}
    int markSubmission(const Session&, int submissionId, double rawMark, const std::string& feedback,
                       const std::string& reason);

  private:
    const repo::ISubmissionRepository& submissions_;
    repo::IMarkRepository& marks_;
    const repo::IModerationRepository& moderation_;
    repo::ITransactionManager& tx_;
    const AuthorizationService& authz_;
    AuditService& audit_;
    const IPenaltyStrategy& penalty_;
};

// FR-15, FR-17 (UC-03): department moderation queue, decisions and explicit release.
class ModerationService {
  public:
    ModerationService(repo::IModerationRepository& moderation, const AuthorizationService& authz)
        : moderation_(moderation), authz_(authz) {}
    std::vector<ModerationItemView> moderationQueue(const Session&) const;
    void moderate(const Session&, int markId, bool approve, const std::string& reason);
    void release(const Session&, int markId);

  private:
    int requireCurrentMarkInDepartment(const Session&, int markId) const;
    repo::IModerationRepository& moderation_;
    const AuthorizationService& authz_;
};

// FR-16, FR-18: students see only their own released marks and feedback.
class ResultQueryService {
  public:
    ResultQueryService(const repo::IModerationRepository& moderation, const AuthorizationService& authz)
        : moderation_(moderation), authz_(authz) {}
    std::vector<ResultView> releasedResults(const Session&) const;

  private:
    const repo::IModerationRepository& moderation_;
    const AuthorizationService& authz_;
};

} // namespace lms::services
