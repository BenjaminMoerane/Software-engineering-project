// AssignmentService, ExtensionService and SubmissionService.
#include "services/DeadlineEvaluator.hpp"
#include "services/Services.hpp"
#include "services/Validation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <random>
#include <stdexcept>

namespace lms::services {

int AssignmentService::createAssignment(const Session& s, int courseId, const std::string& title,
                                        const std::string& description, const std::string& deadline, double maxMark) {
    authz_.requireRole(s, Role::Lecturer);
    authz_.requireLecturerAssigned(s.userId, courseId);
    validation::nonblank(title, "Assignment title");
    validation::isoUtc(deadline, "Deadline");
    if (!std::isfinite(maxMark) || maxMark <= 0)
        throw std::invalid_argument("Maximum mark must be positive and finite");
    return courses_.insertAssignment({courseId, s.userId, title, description, deadline, maxMark});
}

std::vector<AssignmentView> AssignmentService::listAssignments(const Session& s) const {
    authz_.requireValidSession(s);
    return courses_.listAssignmentsVisibleTo(s.userId, domain::roleName(s.role), s.departmentId);
}

void ExtensionService::grantIndividualExtension(const Session& s, int assignmentId, int studentId,
                                                const std::string& revisedDeadline, const std::string& reason) {
    authz_.requireRole(s, Role::Lecturer);
    validation::isoUtc(revisedDeadline, "Revised deadline");
    validation::nonblank(reason, "Extension reason");
    const auto assignment = courses_.findActiveAssignment(assignmentId);
    if (!assignment)
        throw std::runtime_error("Assignment not found or inactive");
    authz_.requireLecturerAssigned(s.userId, assignment->courseId);
    if (!authz_.isStudentEnrolled(studentId, assignment->courseId))
        throw std::invalid_argument("Student is not enrolled in the assignment course");
    // Both values are validated, normalised ISO-8601 UTC strings, so lexical order equals time order.
    if (revisedDeadline <= assignment->deadline)
        throw std::invalid_argument("Revised deadline must be later than original deadline");
    courses_.upsertExtension(assignmentId, studentId, s.userId, revisedDeadline, reason);
}

int ExtensionService::grantClassExtension(const Session& s, int assignmentId, const std::string& revisedDeadline,
                                          const std::string& reason) {
    authz_.requireRole(s, Role::Lecturer);
    validation::isoUtc(revisedDeadline, "Revised deadline");
    validation::nonblank(reason, "Extension reason");
    const auto assignment = courses_.findActiveAssignment(assignmentId);
    if (!assignment)
        throw std::runtime_error("Assignment not found or inactive");
    authz_.requireLecturerAssigned(s.userId, assignment->courseId);
    const std::int64_t revisedAt = DeadlineEvaluator::toEpochSeconds(revisedDeadline);
    if (revisedAt <= DeadlineEvaluator::toEpochSeconds(assignment->deadline))
        throw std::invalid_argument("Revised deadline must be later than original deadline");
    // CR1-D3: a class-wide extension may only move the deadline later; every change is kept as a new row.
    if (const auto current = courses_.findClassExtensionDeadline(assignmentId);
        current && revisedAt <= DeadlineEvaluator::toEpochSeconds(*current))
        throw std::invalid_argument("A class-wide extension can only move the deadline later than " + *current);
    return courses_.insertClassExtension(assignmentId, s.userId, revisedDeadline, reason);
}

int SubmissionService::submit(const Session& s, int assignmentId, const std::string& contentRef) {
    authz_.requireRole(s, Role::Student);
    const std::string submittedAt = clock_(); // server-generated timestamp; never taken from the student
    validation::isoUtc(submittedAt, "System submission timestamp");
    validation::nonblank(contentRef, "Submission content reference");
    const auto assignment = courses_.findActiveAssignment(assignmentId);
    if (!assignment)
        throw std::runtime_error("Assignment not found or inactive");
    if (!authz_.isStudentEnrolled(s.userId, assignment->courseId))
        throw std::runtime_error("Access denied: student is not enrolled in this course");

    // Applicable deadline (BR-11): latest of the assignment deadline, the class-wide extension and the student's
    // individual extension. The version stores this deadline as a snapshot (CR1-D4).
    const std::string deadline = DeadlineEvaluator::applicableDeadline(
        assignment->deadline, courses_.findExtensionDeadline(assignmentId, s.userId),
        courses_.findClassExtensionDeadline(assignmentId));
    const int days = DeadlineEvaluator::latenessDays(submittedAt, deadline);

    const auto prior = submissions_.find(assignmentId, s.userId);
    const int versionNo = prior ? prior->currentVersionNo + 1 : 1;
    if (versionNo > 1 && days > 0)
        throw std::runtime_error("Resubmission is only permitted before the applicable deadline");

    auto tx = tx_.begin();
    const int submissionId =
        submissions_.saveVersion({assignmentId, s.userId, prior ? prior->submissionId : 0, versionNo, submittedAt,
                                  contentRef, deadline, days, DeadlineEvaluator::classify(days)});
    tx->commit();
    return submissionId;
}

int SubmissionService::submitFile(const Session& s, int assignmentId, const std::filesystem::path& sourceFile,
                                  const std::filesystem::path& storageDirectory) {
    authz_.requireRole(s, Role::Student);
    if (!std::filesystem::is_regular_file(sourceFile))
        throw std::invalid_argument("Submission must be an existing regular file");
    const auto size = std::filesystem::file_size(sourceFile);
    constexpr std::uintmax_t maxBytes = std::uintmax_t{20} * 1024 * 1024;
    if (size == 0 || size > maxBytes)
        throw std::invalid_argument("Submission file must be between 1 byte and 20 MiB");
    std::string ext = sourceFile.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::array<const char*, 8> allowed = {".pdf", ".docx", ".doc", ".txt",
                                                       ".zip", ".png",  ".jpg", ".jpeg"};
    if (std::find_if(allowed.begin(), allowed.end(), [&](const char* x) { return ext == x; }) == allowed.end())
        throw std::invalid_argument("Unsupported file type. Allowed: PDF, DOC/DOCX, TXT, ZIP, PNG, JPG/JPEG");
    std::filesystem::create_directories(storageDirectory);
    std::random_device rd;
    std::string token;
    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
        token += hex[rd() & 15U];
    const auto destination = storageDirectory / ("submission_" + token + ext);
    std::error_code ec;
    std::filesystem::copy_file(sourceFile, destination, std::filesystem::copy_options::none, ec);
    if (ec)
        throw std::runtime_error("Could not store submission file: " + ec.message());
    try {
        return submit(s, assignmentId, std::filesystem::absolute(destination).string());
    } catch (...) {
        std::filesystem::remove(destination, ec); // no orphaned file when the database write fails
        throw;
    }
}

std::vector<SubmissionView> SubmissionService::submissionsForMarking(const Session& s) const {
    authz_.requireRole(s, Role::Lecturer);
    return submissions_.markingQueueFor(s.userId);
}

} // namespace lms::services
