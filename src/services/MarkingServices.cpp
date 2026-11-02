// AuditService and MarkingService.
#include "services/Services.hpp"
#include "services/Validation.hpp"

#include <cmath>
#include <stdexcept>

namespace lms::services {

void AuditService::recordMarkChange(int markId, int actorId, std::optional<double> oldMark, double newMark,
                                    const std::string& reason) {
    validation::nonblank(reason, "Mark-change reason");
    marks_.appendAudit({markId, actorId, oldMark, newMark, reason});
}

int MarkingService::markSubmission(const Session& s, int submissionId, double rawMark, const std::string& feedback,
                                   const std::string& reason) {
    authz_.requireRole(s, Role::Lecturer);
    validation::nonblank(feedback, "Feedback");
    validation::nonblank(reason, "Mark-change reason");
    const auto ctx = submissions_.findMarkingContext(submissionId);
    if (!ctx)
        throw std::runtime_error("Submission not found");
    authz_.requireLecturerAssigned(s.userId, ctx->courseId);
    if (!std::isfinite(rawMark) || rawMark < 0 || rawMark > ctx->maxMark)
        throw std::invalid_argument("Raw mark must be between zero and maximum mark");

    // The penalty is applied exactly once, here, from the lateness recorded on the version being marked.
    const PenaltyResult penalty = penalty_.calculate(ctx->latenessDays, ctx->maxMark);
    const domain::MarkWrite write{submissionId, ctx->versionId, s.userId, rawMark, penalty.penalty,
                          applyPenalty(rawMark, penalty), feedback};

    // Guards, mark/feedback write and audit event share one transaction: any failure leaves no partial change.
    auto tx = tx_.begin();
    const auto current = marks_.findCurrent(submissionId);
    int markId = 0;
    std::optional<double> oldMark;
    if (current) {
        markId = current->markId;
        oldMark = current->finalMark;
        if (moderation_.isReleased(markId))
            throw std::runtime_error("Post-release mark corrections are blocked pending Gate 2 decision OD-08");
        if (moderation_.latestDecision(markId).value_or("") == "Approved")
            throw std::runtime_error("Mark is approved by the HoD; it must be returned for correction (fresh "
                                     "moderation) before it can be changed");
        marks_.updateCurrent(markId, write);
    } else {
        markId = marks_.insertCurrent(write);
    }
    audit_.recordMarkChange(markId, s.userId, oldMark, write.finalMark, reason);
    tx->commit();
    return markId;
}

} // namespace lms::services
