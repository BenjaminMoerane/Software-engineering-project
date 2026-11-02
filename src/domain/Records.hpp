#pragma once
// Plain data carried between the application services and the repository layer.
// These are value types only: they hold no SQL and no behaviour, so both layers can depend on them
// without the services knowing how the data is stored (Phase 3 SDD section 3, Repository/DAO pattern).
#include <optional>
#include <string>

namespace lms::domain {

// ---- read models returned to the presentation layer -------------------------------------------------------------
struct AssignmentView {
    int id{};
    std::string courseCode, title, deadline;
    double maxMark{};
    std::string extensionDeadline;      // individual extension for the viewing student (FR-21), empty if none
    std::string classExtensionDeadline; // class-wide extension in force (CR1, FR-23), empty if none
};
struct SubmissionView {
    int submissionId{}, versionId{}, studentId{}, assignmentId{}, versionNo{}, latenessDays{};
    std::string studentName, assignmentTitle, submittedAt, lateStatus, contentRef;
    double maxMark{};
};
struct ModerationItemView {
    int markId{}, submissionId{};
    std::string studentName, courseCode, assignmentTitle, lateStatus, feedback, state;
    double rawMark{}, penalty{}, finalMark{}, maxMark{};
};
struct ResultView {
    int assignmentId{};
    std::string assignmentTitle, courseCode, feedback;
    double mark{}, maxMark{};
};

// ---- records used by the repository interfaces -------------------------------------------------------------------
struct UserCredentials { // row needed by AuthenticationService::login
    int userId{};
    std::string username, fullName, role;
    int departmentId{};
    std::string passwordHash, passwordSalt;
    bool active{};
};
struct AccountState { // stored role/department/active flag used to validate a Session
    bool active{};
    std::string role;
    int departmentId{};
};
struct NewUser {
    std::string username, passwordHash, passwordSalt, role, fullName;
    int departmentId{}; // 0 means "no department" (stored as NULL)
};
struct AssignmentInfo { // active assignment facts needed by submission and extension rules
    int courseId{};
    std::string deadline;
    double maxMark{};
};
struct NewAssignment {
    int courseId{}, createdBy{};
    std::string title, description, deadline;
    double maxMark{};
};
struct ExistingSubmission {
    int submissionId{}, currentVersionNo{};
};
struct NewSubmissionVersion {
    int assignmentId{}, studentId{};
    int submissionId{}; // 0 when this is the first version and the logical submission must be created
    int versionNo{};
    std::string submittedAt, contentRef, deadlineUsed;
    int latenessDays{};
    std::string lateStatus;
};
struct MarkingContext { // the current version of a submission, as needed to mark it
    int courseId{};
    double maxMark{};
    int versionId{}, latenessDays{};
    std::string lateStatus;
};
struct CurrentMark {
    int markId{};
    double finalMark{};
};
struct MarkWrite {
    int submissionId{}, versionId{}, enteredBy{};
    double rawMark{}, penaltyAmount{}, finalMark{};
    std::string feedback;
};
struct MarkAuditEntry {
    int markId{}, actorId{};
    std::optional<double> oldMark; // empty for the first mark
    double newMark{};
    std::string reason;
};
struct ModerationQueueRow {
    ModerationItemView item;
    std::string latestDecision; // empty when the mark has never been moderated
    bool correctedSinceLatestDecision{};
};

} // namespace lms::domain
