// ModerationService and ResultQueryService (UC-03, FR-15 to FR-18).
#include "services/Services.hpp"
#include "services/Validation.hpp"

#include <stdexcept>

namespace lms::services {

std::vector<ModerationItemView> ModerationService::moderationQueue(const Session& s) const {
    authz_.requireRole(s, Role::HeadOfDepartment);
    std::vector<ModerationItemView> out;
    for (auto& row : moderation_.unreleasedCurrentMarksFor(s.departmentId)) {
        // A returned mark is back with the lecturer until they re-mark it after the decision.
        if (row.latestDecision == "ReturnedForCorrection" && !row.correctedSinceLatestDecision)
            continue;
        row.item.state = row.latestDecision == "Approved" ? "ApprovedAwaitingRelease" : "PendingModeration";
        out.push_back(std::move(row.item));
    }
    return out;
}

int ModerationService::requireCurrentMarkInDepartment(const Session& s, int markId) const {
    const auto department = moderation_.departmentOfCurrentMark(markId);
    if (!department)
        throw std::runtime_error("Current mark not found");
    if (*department != s.departmentId)
        throw std::runtime_error("Access denied: mark is outside HoD department");
    return *department;
}

void ModerationService::moderate(const Session& s, int markId, bool approve, const std::string& reason) {
    authz_.requireRole(s, Role::HeadOfDepartment);
    validation::nonblank(reason, "Moderation reason");
    requireCurrentMarkInDepartment(s, markId);
    if (moderation_.isReleased(markId))
        throw std::runtime_error("Moderation of released marks is blocked pending Gate 2 decision OD-08");
    moderation_.insertDecision(markId, s.userId, approve ? "Approved" : "ReturnedForCorrection", reason);
}

void ModerationService::release(const Session& s, int markId) {
    authz_.requireRole(s, Role::HeadOfDepartment);
    requireCurrentMarkInDepartment(s, markId);
    // Approval and release are separate events (Principal + HoD statements): only the latest decision counts.
    if (moderation_.latestDecision(markId).value_or("") != "Approved")
        throw std::runtime_error("Mark cannot be released without latest HoD approval");
    moderation_.insertRelease(markId, s.userId);
}

std::vector<ResultView> ResultQueryService::releasedResults(const Session& s) const {
    authz_.requireRole(s, Role::Student);
    return moderation_.releasedResultsFor(s.userId); // ownership enforced by the student-id filter
}

} // namespace lms::services
